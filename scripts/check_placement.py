#!/usr/bin/env python3
"""
Variable Placement Problem Detector for RP2040/FreeRTOS
Analyzes ELF files to detect potential memory placement issues
"""

import subprocess
import sys
import re
from dataclasses import dataclass
from typing import List, Tuple, Optional

@dataclass
class MemoryRegion:
    name: str
    start: int
    size: int
    attributes: str

@dataclass
class Variable:
    name: str
    address: int
    size: int
    section: str
    type: str

class MemoryAnalyzer:
    # RP2040 Memory Map
    RAM_START = 0x20000000
    RAM_END = 0x20042000  # 264KB
    FLASH_START = 0x10000000
    FLASH_END = 0x10200000  # 2MB
    
    # Striped RAM regions (for SMP)
    STRIPED_RAM_START = 0x20000000
    STRIPED_RAM_END = 0x20040000
    
    # Scratch regions (used by multicore)
    SCRATCH_X_START = 0x20040000
    SCRATCH_X_END = 0x20041000
    SCRATCH_Y_START = 0x20041000
    SCRATCH_Y_END = 0x20042000
    
    def __init__(self, elf_file: str):
        self.elf_file = elf_file
        self.problems = []
        self.warnings = []
        
    def run_command(self, cmd: List[str]) -> str:
        """Run command and return output"""
        result = subprocess.run(cmd, capture_output=True, text=True)
        if result.returncode != 0:
            raise Exception(f"Command failed: {' '.join(cmd)}\n{result.stderr}")
        return result.stdout
    
    def get_variable_info(self, var_name: str) -> Optional[Variable]:
        """Extract variable information from ELF"""
        # Get symbol info
        nm_output = self.run_command(['arm-none-eabi-nm', '-S', self.elf_file])
        
        for line in nm_output.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[-1] == var_name:
                addr = int(parts[0], 16)
                size = int(parts[1], 16) if len(parts) > 3 else 4
                var_type = parts[-2]
                
                # Get section info
                objdump = self.run_command(['arm-none-eabi-objdump', '-t', self.elf_file])
                section = "unknown"
                for obj_line in objdump.splitlines():
                    if var_name in obj_line:
                        obj_parts = obj_line.split()
                        if len(obj_parts) > 3:
                            section = obj_parts[3]
                            break
                
                return Variable(var_name, addr, size, section, var_type)
        
        return None
    
    def check_volatile_placement(self, var: Variable) -> List[str]:
        """Check if volatile variable is properly placed"""
        problems = []
        
        # Check 1: Volatile in Flash
        if self.FLASH_START <= var.address < self.FLASH_END:
            problems.append(f"CRITICAL: Volatile variable '{var.name}' placed in FLASH (0x{var.address:08x})")
            problems.append("  → Flash is read-only at runtime, volatile won't work!")
            problems.append("  → Solution: Force placement in RAM section")
        
        # Check 2: Alignment for atomic operations
        if var.address % 4 != 0:
            problems.append(f"ERROR: Variable '{var.name}' not 4-byte aligned (0x{var.address:08x})")
            problems.append("  → Required for atomic operations in SMP")
            problems.append("  → Solution: Add __attribute__((aligned(4)))")
        
        # Check 3: In scratch regions (bad for SMP)
        if self.SCRATCH_X_START <= var.address < self.SCRATCH_Y_END:
            problems.append(f"WARNING: Variable '{var.name}' in scratch RAM (0x{var.address:08x})")
            problems.append("  → Scratch regions have special multicore behavior")
            problems.append("  → May cause cache coherency issues")
        
        return problems
    
    def check_memory_conflicts(self, var: Variable) -> List[str]:
        """Check for conflicts with heap/stack"""
        problems = []
        
        # Get heap and stack boundaries
        symbols = self.run_command(['arm-none-eabi-nm', self.elf_file])
        
        heap_start = heap_end = stack_start = stack_end = None
        
        for line in symbols.splitlines():
            parts = line.split()
            if len(parts) >= 3:
                if parts[2] == '_heap_start':
                    heap_start = int(parts[0], 16)
                elif parts[2] == '_heap_end' or parts[2] == '__HeapLimit':
                    heap_end = int(parts[0], 16)
                elif parts[2] == '__StackTop' or parts[2] == '_stack_top':
                    stack_end = int(parts[0], 16)
                elif parts[2] == '__StackLimit' or parts[2] == '_stack_bottom':
                    stack_start = int(parts[0], 16)
        
        # Check heap collision
        if heap_start and heap_end:
            if heap_start <= var.address < heap_end:
                problems.append(f"CRITICAL: Variable '{var.name}' overlaps with heap!")
                problems.append(f"  → Variable at 0x{var.address:08x}, heap: 0x{heap_start:08x}-0x{heap_end:08x}")
            elif abs(var.address - heap_start) < 256:
                problems.append(f"WARNING: Variable '{var.name}' very close to heap (distance: {abs(var.address - heap_start)} bytes)")
        
        # Check stack collision
        if stack_start and stack_end:
            if stack_start <= var.address < stack_end:
                problems.append(f"CRITICAL: Variable '{var.name}' overlaps with stack!")
            elif abs(var.address - stack_start) < 1024:
                problems.append(f"WARNING: Variable '{var.name}' close to stack (distance: {abs(var.address - stack_start)} bytes)")
        
        return problems
    
    def check_freertos_specific(self, var: Variable) -> List[str]:
        """Check FreeRTOS-specific issues"""
        problems = []
        
        # Check if in FreeRTOS heap region
        symbols = self.run_command(['arm-none-eabi-nm', self.elf_file])
        
        for line in symbols.splitlines():
            parts = line.split()
            if len(parts) >= 3 and ('ucHeap' in parts[2] or 'xHeap' in parts[2]):
                freertos_heap_addr = int(parts[0], 16)
                # Assume default configTOTAL_HEAP_SIZE
                if freertos_heap_addr <= var.address < freertos_heap_addr + 0x10000:
                    problems.append(f"ERROR: Variable '{var.name}' inside FreeRTOS heap!")
                    problems.append("  → Will be overwritten by dynamic allocation")
        
        # Check section attributes
        if var.section == '.bss' and 'volatile' in var.name.lower():
            problems.append(f"WARNING: Volatile variable '{var.name}' in .bss section")
            problems.append("  → May not be initialized before FreeRTOS starts")
            problems.append("  → Solution: Place in .data with explicit init value")
        
        return problems
    
    def analyze_variable(self, var_name: str) -> Tuple[List[str], List[str]]:
        """Analyze a specific variable for problems"""
        var = self.get_variable_info(var_name)
        if not var:
            return [f"Variable '{var_name}' not found in ELF"], []
        
        all_problems = []
        all_warnings = []
        
        # Run all checks
        problems = self.check_volatile_placement(var)
        all_problems.extend([p for p in problems if 'CRITICAL' in p or 'ERROR' in p])
        all_warnings.extend([p for p in problems if 'WARNING' in p])
        
        problems = self.check_memory_conflicts(var)
        all_problems.extend([p for p in problems if 'CRITICAL' in p or 'ERROR' in p])
        all_warnings.extend([p for p in problems if 'WARNING' in p])
        
        problems = self.check_freertos_specific(var)
        all_problems.extend([p for p in problems if 'CRITICAL' in p or 'ERROR' in p])
        all_warnings.extend([p for p in problems if 'WARNING' in p])
        
        return all_problems, all_warnings
    
    def generate_report(self, var_name: str):
        """Generate full analysis report"""
        print(f"\n{'='*60}")
        print(f"Memory Placement Analysis: {var_name}")
        print(f"{'='*60}\n")
        
        var = self.get_variable_info(var_name)
        if not var:
            print(f"ERROR: Variable '{var_name}' not found!")
            return
        
        # Basic info
        print(f"Variable Information:")
        print(f"  Name:     {var.name}")
        print(f"  Address:  0x{var.address:08x}")
        print(f"  Size:     {var.size} bytes")
        print(f"  Section:  {var.section}")
        print(f"  Type:     {var.type}")
        
        # Memory region
        if self.RAM_START <= var.address < self.RAM_END:
            print(f"  Region:   RAM (OK for volatile)")
        elif self.FLASH_START <= var.address < self.FLASH_END:
            print(f"  Region:   FLASH (PROBLEM for volatile!)")
        else:
            print(f"  Region:   Unknown")
        
        # Run analysis
        problems, warnings = self.analyze_variable(var_name)
        
        if problems:
            print(f"\n❌ PROBLEMS FOUND ({len(problems)}):")
            for p in problems:
                print(f"  {p}")
        
        if warnings:
            print(f"\n⚠️  WARNINGS ({len(warnings)}):")
            for w in warnings:
                print(f"  {w}")
        
        if not problems and not warnings:
            print("\n✅ No problems detected!")
        
        # Recommendations
        print(f"\n{'='*60}")
        print("RECOMMENDATIONS:")
        print(f"{'='*60}")
        
        if problems or warnings:
            print("\n1. Force RAM placement in your source:")
            print(f"   __attribute__((section(\".data\"))) volatile uint32_t {var_name} = 0;")
            
            print("\n2. Or create a custom section in linker script:")
            print("   .volatile_data : {")
            print("       . = ALIGN(4);")
            print("       *(.volatile_data)")
            print("   } > RAM")
            print(f"   Then: __attribute__((section(\".volatile_data\"))) volatile uint32_t {var_name};")
            
            print("\n3. For SMP/multicore safety:")
            print(f"   volatile uint32_t {var_name} __attribute__((aligned(4)));")
            print("   // Access with atomic operations:")
            print(f"   uint32_t val = __atomic_load_n(&{var_name}, __ATOMIC_SEQ_CST);")

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 check_placement.py <elf_file> <variable_name>")
        sys.exit(1)
    
    elf_file = sys.argv[1]
    var_name = sys.argv[2]
    
    analyzer = MemoryAnalyzer(elf_file)
    analyzer.generate_report(var_name)

if __name__ == "__main__":
    main()