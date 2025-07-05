# Complete README.md Enhancement Prompt for Embedded C Projects

**⚠️ CRITICAL INSTRUCTION: PRESERVE ALL EXISTING CONTENT AND SYNCHRONIZE WITH CODEBASE**

**Instructions:** Use this prompt to enhance and organize an existing README.md file for your embedded C project. The AI assistant must **PRESERVE ALL EXISTING INFORMATION**, **SYNCHRONIZE WITH ACTUAL CODEBASE**, and **REMOVE OUTDATED CONTENT** while only adding, organizing, and enhancing the current documentation following embedded systems best practices.

**PRESERVATION AND SYNCHRONIZATION REQUIREMENTS:**
- ✅ **KEEP ALL** existing text, code examples, and technical details that match current implementation
- ✅ **MAINTAIN ALL** current project-specific information that remains valid
- ✅ **PRESERVE ALL** existing links, references, and acknowledgments that are still relevant
- ✅ **RETAIN ALL** current configuration examples and code snippets that work with current codebase
- ✅ **ORGANIZE** existing content into the structured format below
- ✅ **ENHANCE** with additional embedded C best practices and missing sections
- ✅ **ADD** complementary information that doesn't conflict with existing content
- ❌ **REMOVE** outdated sections, deprecated functions, and obsolete information
- ❌ **DELETE** references to removed features, changed APIs, and discontinued protocols
- ❌ **ELIMINATE** documentation for code that no longer exists in the project

**MANDATORY CODEBASE ANALYSIS REQUIREMENTS:**
- 🔍 **ANALYZE** the actual source code to identify discrepancies with existing README
- 🔍 **INVESTIGATE** Doxygen comments and documentation in the codebase
- 🔍 **COMPARE** documented APIs with actual function signatures
- 🔍 **VERIFY** command lists, protocols, and features against implementation
- 🔍 **UPDATE** outdated information based on current code state
- 🔍 **INTEGRATE** Doxygen-generated documentation into README structure
- 🔍 **REMOVE** documentation for functions/features no longer in codebase
- 🔍 **ADD** documentation for new functions/features found in code

**CONDITIONAL SECTION REQUIREMENTS:**
- ⚠️ **ONLY INCLUDE sections that are relevant to the actual codebase**
- ⚠️ **DO NOT ADD** sections for features not implemented (e.g., no testing section if no tests exist)
- ⚠️ **FOCUS ON** actual software features, implemented protocols, and real usage patterns
- ⚠️ **DOCUMENT** actual commands, APIs, and system behavior as implemented
- ⚠️ **SEARCH FOR** devcontainer configurations to simplify development setup

---

## Development Environment Detection and Setup

### 🐳 Devcontainer Integration Analysis

**FIRST PRIORITY: Check for development container configuration to simplify onboarding:**

#### 1. Devcontainer Detection Script
```bash
#!/bin/bash
# detect_devcontainer.sh - Check for containerized development setup

echo "=== Development Container Detection ==="

# Check for devcontainer configuration
if [ -d ".devcontainer" ]; then
    echo "✅ DevContainer found in .devcontainer/"
    if [ -f ".devcontainer/devcontainer.json" ]; then
        echo "   - Configuration: devcontainer.json"
        # Extract container image and features
        grep -E "(image|dockerFile)" .devcontainer/devcontainer.json
    fi
    if [ -f ".devcontainer/Dockerfile" ]; then
        echo "   - Custom Dockerfile detected"
    fi
    echo ""
    echo "🚀 QUICK START WITH DEVCONTAINER:"
    echo "   1. Install VS Code + Dev Containers extension"
    echo "   2. Open project in VS Code"
    echo "   3. Click 'Reopen in Container' when prompted"
    echo "   4. Everything will be configured automatically!"
    echo ""
fi

# Check for Docker Compose development setup
if [ -f "docker-compose.yml" ] || [ -f "docker-compose.yaml" ]; then
    echo "🐳 Docker Compose development setup found"
    echo "   Run: docker-compose up -d"
fi

# Check for Dockerfile for manual container builds
if [ -f "Dockerfile" ] && [ ! -d ".devcontainer" ]; then
    echo "🐳 Standalone Dockerfile found"
    echo "   Build: docker build -t project-dev ."
    echo "   Run: docker run -it -v \$(pwd):/workspace project-dev"
fi

# Check for GitHub Codespaces configuration
if [ -d ".github" ] && [ -f ".github/codespaces/devcontainer.json" ]; then
    echo "☁️ GitHub Codespaces configuration found"
    echo "   - Can be opened directly in GitHub Codespaces"
fi

# Check for traditional development setup
if [ -f "Vagrantfile" ]; then
    echo "📦 Vagrant development environment found"
    echo "   Run: vagrant up && vagrant ssh"
fi

echo "=== Recommended Development Setup ==="
if [ -d ".devcontainer" ]; then
    echo "🎯 EASIEST: Use VS Code with Dev Containers extension"
    echo "   All dependencies and tools pre-configured!"
else
    echo "⚙️ MANUAL: Follow manual installation instructions below"
fi
```

#### 2. Devcontainer Feature Detection
```bash
#!/bin/bash
# analyze_devcontainer.sh - Extract devcontainer configuration details

if [ -f ".devcontainer/devcontainer.json" ]; then
    echo "=== DevContainer Configuration Analysis ==="
    
    # Extract base image
    echo "Base Image:"
    grep -E "\"image\":" .devcontainer/devcontainer.json | sed 's/.*"image": *"\([^"]*\)".*/  - \1/'
    
    # Extract features
    echo -e "\nConfigured Features:"
    if grep -q "\"features\":" .devcontainer/devcontainer.json; then
        grep -A 20 "\"features\":" .devcontainer/devcontainer.json | grep -E "\"[^\"]+\":" | sed 's/.*"\([^"]*\)".*/  - \1/'
    fi
    
    # Extract VS Code extensions
    echo -e "\nPre-installed Extensions:"
    if grep -q "\"extensions\":" .devcontainer/devcontainer.json; then
        grep -A 10 "\"extensions\":" .devcontainer/devcontainer.json | grep -E "\"[^\"]+\"" | sed 's/.*"\([^"]*\)".*/  - \1/'
    fi
    
    # Extract port forwards
    echo -e "\nPort Forwarding:"
    if grep -q "\"forwardPorts\":" .devcontainer/devcontainer.json; then
        grep -A 5 "\"forwardPorts\":" .devcontainer/devcontainer.json | grep -E "[0-9]+" | sed 's/.* \([0-9]*\).*/  - Port \1/'
    fi
    
    # Extract post-create commands
    echo -e "\nSetup Commands:"
    if grep -q "\"postCreateCommand\":" .devcontainer/devcontainer.json; then
        grep "\"postCreateCommand\":" .devcontainer/devcontainer.json | sed 's/.*"postCreateCommand": *"\([^"]*\)".*/  - \1/'
    fi
fi
```

#### 3. Development Environment Recommendations
```markdown
## 🚀 Quick Start Guide (PRIORITY: Check for easiest setup method)

### Option 1: DevContainer (RECOMMENDED if available)
If `.devcontainer/` directory exists in this project:

1. **Prerequisites:**
   - VS Code with Dev Containers extension
   - Docker Desktop installed and running

2. **One-Click Setup:**
   ```bash
   # Clone and open in VS Code
   git clone <repository-url>
   code <project-directory>
   # Click "Reopen in Container" when prompted
   ```

3. **What you get automatically:**
   - ✅ Correct ARM toolchain (gcc-arm-none-eabi)
   - ✅ Debugging tools (OpenOCD, GDB)
   - ✅ Code analysis (clang-format, cppcheck)
   - ✅ Build system (Make, CMake)
   - ✅ VS Code extensions for embedded development
   - ✅ Hardware debugging configuration

### Option 2: Manual Installation (if no devcontainer)
[Include traditional setup instructions if devcontainer not available]
```

---

## Documentation Maintenance and Synchronization

### 🔄 Automated README-Code Synchronization

**CRITICAL: The README must always reflect the current state of the codebase**

#### 1. Code Reality Check Script
```bash
#!/bin/bash
# readme_sync_check.sh - Verify README matches actual implementation

echo "=== README-Codebase Synchronization Check ==="

# Function existence verification
echo "🔍 Checking documented functions against codebase..."
if [ -f "README.md" ]; then
    # Extract function names from README
    README_FUNCTIONS=$(grep -oE "\b[a-zA-Z_][a-zA-Z0-9_]*\s*\(" README.md | sort | uniq)
    
    # Extract actual functions from source code
    CODE_FUNCTIONS=$(find src/ include/ -name "*.c" -o -name "*.h" | xargs grep -h "^[a-zA-Z_][a-zA-Z0-9_]*\s\+[a-zA-Z_][a-zA-Z0-9_]*\s*(" | grep -v static | sort | uniq)
    
    echo "📋 Functions documented in README but NOT in code (REMOVE FROM README):"
    comm -23 <(echo "$README_FUNCTIONS") <(echo "$CODE_FUNCTIONS") | head -10
    
    echo "📋 Functions in code but NOT documented in README (ADD TO README):"
    comm -13 <(echo "$README_FUNCTIONS") <(echo "$CODE_FUNCTIONS") | head -10
fi

# Command verification
echo -e "\n🔍 Checking documented commands against parser..."
if find . -name "*.c" | xargs grep -l "cmd_\|command" > /dev/null; then
    echo "📋 Commands found in parser:"
    find . -name "*.c" | xargs grep -h "cmd_\|command" | grep -v ".o:" | head -10
    
    echo "📋 Verify these match README command documentation"
fi

# Configuration parameter verification
echo -e "\n🔍 Checking configuration parameters..."
CONFIG_DEFINES=$(find . -name "*.h" | xargs grep -h "#define.*CONFIG\|#define.*MAX_\|#define.*DEFAULT_" | wc -l)
echo "📋 Found $CONFIG_DEFINES configuration parameters in code"
echo "📋 Verify README configuration section includes all current parameters"

# Protocol implementation check
echo -e "\n🔍 Checking protocol implementations..."
PROTOCOLS=$(find . -name "*.c" | xargs grep -l "uart\|spi\|i2c\|can\|modbus\|ethernet" | wc -l)
echo "📋 Found protocol-related files: $PROTOCOLS"
echo "📋 Verify README documents only implemented protocols"

echo -e "\n⚠️  ACTION REQUIRED:"
echo "   1. Remove documentation for functions not in code"
echo "   2. Add documentation for new functions found in code"
echo "   3. Update command reference to match parser implementation"
echo "   4. Verify configuration parameters match code defaults"
echo "   5. Remove references to unimplemented protocols"
echo "   6. Add documentation for newly implemented features"
```

#### 2. Doxygen Integration and Extraction
```bash
#!/bin/bash
# extract_doxygen_docs.sh - Extract documentation from code comments

echo "=== Extracting Documentation from Code ==="

# Extract API functions with Doxygen comments
echo "📖 API Functions with Documentation:"
grep -r -A 10 "\/\*\*" src/ include/ | grep -E "@brief|@param|@return" | while read line; do
    echo "   $line"
done

# Extract module documentation
echo -e "\n📖 Module Documentation:"
grep -r "@file\|@defgroup\|@addtogroup" src/ include/ | head -10

# Extract configuration documentation
echo -e "\n📖 Configuration Parameters:"
grep -r -B 2 -A 2 "#define.*CONFIG\|#define.*MAX\|#define.*DEFAULT" include/ | grep -E "\/\*\*|@brief"

# Extract command documentation
echo -e "\n📖 Command Documentation:"
grep -r -B 5 -A 5 "cmd_\|command" src/ | grep -E "\/\*\*|@brief|@param"

echo -e "\n✅ Integration Instructions:"
echo "   1. Copy function descriptions from @brief comments"
echo "   2. Use @param information for parameter documentation"
echo "   3. Include @return information for API reference"
echo "   4. Extract @note and @warning for important information"
echo "   5. Use @code blocks for usage examples"
```

#### 3. Outdated Content Detection and Removal
```bash
#!/bin/bash
# detect_outdated_content.sh - Find content that needs removal

echo "=== Detecting Outdated README Content ==="

if [ -f "README.md" ]; then
    # Check for references to removed files
    echo "🗑️ Checking for references to missing files..."
    README_FILES=$(grep -oE "[a-zA-Z0-9_/.-]+\.(c|h|py|sh)" README.md | sort | uniq)
    for file in $README_FILES; do
        if [ ! -f "$file" ]; then
            echo "   ❌ Referenced file not found: $file"
        fi
    done
    
    # Check for outdated version numbers
    echo -e "\n🗑️ Checking for version inconsistencies..."
    README_VERSIONS=$(grep -oE "v?[0-9]+\.[0-9]+\.[0-9]+" README.md | sort | uniq)
    if [ -f "version.h" ]; then
        CODE_VERSION=$(grep -oE "v?[0-9]+\.[0-9]+\.[0-9]+" version.h | head -1)
        echo "   README versions: $README_VERSIONS"
        echo "   Code version: $CODE_VERSION"
    fi
    
    # Check for deprecated terminology
    echo -e "\n🗑️ Checking for deprecated terminology..."
    DEPRECATED_TERMS=("old_function" "deprecated_api" "legacy_mode" "TODO" "FIXME")
    for term in "${DEPRECATED_TERMS[@]}"; do
        if grep -q "$term" README.md; then
            echo "   ⚠️ Found deprecated term: $term"
            grep -n "$term" README.md | head -3
        fi
    done
    
    # Check for broken internal links
    echo -e "\n🗑️ Checking for broken internal links..."
    INTERNAL_LINKS=$(grep -oE "\[.*\]\(#[^)]+\)" README.md)
    for link in $INTERNAL_LINKS; do
        anchor=$(echo "$link" | sed 's/.*#\([^)]*\).*/\1/')
        if ! grep -q "^#.*$(echo $anchor | tr '-' ' ')" README.md; then
            echo "   ❌ Broken internal link: $link"
        fi
    done
fi

echo -e "\n🧹 Cleanup Actions Required:"
echo "   1. Remove references to deleted files"
echo "   2. Update version numbers to match code"
echo "   3. Remove or update deprecated terminology"
echo "   4. Fix broken internal links"
echo "   5. Remove sections for unimplemented features"
```

### 📊 README Health Check and Maintenance

**Continuous README quality assurance:**

#### Weekly Maintenance Checklist:
- [ ] **Function inventory**: All public functions documented, removed functions deleted from README
- [ ] **Command reference**: Parser commands match README command table exactly
- [ ] **Configuration sync**: All #define parameters documented with correct defaults
- [ ] **Protocol accuracy**: Only implemented protocols documented, deprecated ones removed
- [ ] **Hardware relevance**: Pin assignments and hardware specs match current board design
- [ ] **Build instructions**: Commands tested on clean environment, outdated steps removed
- [ ] **Example validation**: All code examples compile and work with current codebase
- [ ] **Link verification**: All URLs work, internal links point to existing sections
- [ ] **Version consistency**: Version numbers match across README, code, and build system
- [ ] **Devcontainer update**: Configuration reflects current development setup

#### Git Hooks for Documentation Synchronization:
```bash
#!/bin/bash
# pre-commit hook for README synchronization
echo "🔍 Checking README synchronization with codebase..."

# Detect API changes
if git diff --cached --name-only | grep -qE "\.(h|c)$"; then
    echo "⚠️ Source files changed - running README sync check..."
    ./scripts/readme_sync_check.sh
    
    # Count function changes
    NEW_FUNCTIONS=$(git diff --cached | grep -c "^+.*[a-zA-Z_][a-zA-Z0-9_]*\s*(")
    REMOVED_FUNCTIONS=$(git diff --cached | grep -c "^-.*[a-zA-Z_][a-zA-Z0-9_]*\s*(")
    
    if [ $NEW_FUNCTIONS -gt 0 ]; then
        echo "📝 $NEW_FUNCTIONS new functions detected - update README API section"
    fi
    if [ $REMOVED_FUNCTIONS -gt 0 ]; then
        echo "🗑️ $REMOVED_FUNCTIONS functions removed - clean README documentation"
    fi
fi

# Detect command parser changes
if git diff --cached | grep -q "cmd_\|command"; then
    echo "⚠️ Command parser changes detected - update README command reference"
fi

# Detect configuration changes
if git diff --cached | grep -q "#define.*CONFIG\|#define.*MAX\|#define.*DEFAULT"; then
    echo "⚠️ Configuration parameters changed - update README configuration section"
fi

echo "✅ Complete README sync check before committing"
```

---

## Enhanced README Structure with Codebase Synchronization

**STEP 1: MANDATORY CODEBASE ANALYSIS AND SYNCHRONIZATION**

### 🔍 Pre-Documentation Analysis Protocol

**Execute these analysis steps BEFORE organizing README content:**

#### 1. Complete Codebase Inventory
```bash
# Generate complete project inventory
find . -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.hpp" | \
    xargs wc -l | sort -n > codebase_inventory.txt

# List all public API functions
grep -rh "^[a-zA-Z_][a-zA-Z0-9_]*\s\+[a-zA-Z_][a-zA-Z0-9_]*\s*(" src/ include/ | \
    grep -v static | sort > current_api_functions.txt

# Extract all #define constants
grep -rh "#define" include/ | grep -v "^//" | sort > current_defines.txt

# Find all command handlers
grep -rh "cmd_\|command" src/ | grep -E "(function|handler)" | sort > current_commands.txt
```

#### 2. Documentation Gap Analysis
```markdown
## 📋 Documentation Synchronization Report

### Functions Analysis:
- **Total functions in codebase**: [COUNT]
- **Functions documented in README**: [COUNT]
- **Missing documentation**: [LIST NEW FUNCTIONS]
- **Obsolete documentation**: [LIST REMOVED FUNCTIONS]

### Protocol Implementation Status:
- **Implemented protocols**: [SCAN CODE FOR UART/SPI/I2C/CAN/etc.]
- **Documented protocols**: [CHECK README SECTIONS]
- **Documentation gaps**: [MISSING PROTOCOL DOCS]
- **Outdated documentation**: [DEPRECATED PROTOCOL DOCS]

### Command Interface Status:
- **Commands in parser**: [EXTRACT FROM COMMAND TABLES]
- **Commands in README**: [CHECK COMMAND REFERENCE]
- **Syntax discrepancies**: [COMPARE COMMAND FORMATS]
- **Missing commands**: [NEW COMMANDS TO DOCUMENT]

### Configuration Parameters:
- **Parameters in code**: [COUNT #DEFINE CONFIG/MAX/DEFAULT]
- **Parameters in README**: [CHECK CONFIG SECTION]
- **Default value changes**: [COMPARE DEFAULTS]
- **New parameters**: [UNDOCUMENTED SETTINGS]
```

#### 3. Content Relevance Verification
**For EACH section in existing README, verify:**
- ✅ **Still implemented?** - Check if features/functions still exist in code
- ✅ **Still accurate?** - Verify syntax, parameters, behavior match implementation
- ✅ **Still relevant?** - Confirm feature is not deprecated or replaced
- ❌ **Mark for removal** - Flag outdated, incorrect, or obsolete information

**STEP 2: ENHANCED DOCUMENTATION STRUCTURE WITH SYNCHRONIZATION**

### 1. Project Header Section (PRESERVE + ENHANCE WITH DEVCONTAINER)
**PRESERVE existing project title, description, and badges. ADD devcontainer quick-start if available:**

```markdown
# [EXISTING PROJECT NAME] - [Keep existing subtitle]
> [Preserve existing description]

[Keep all existing badges] + [Add environment badges]

## 🚀 Quick Start

### Fastest Setup (if devcontainer available):
1. Open in VS Code with Dev Containers extension
2. Click "Reopen in Container"
3. Everything configured automatically!

[Include devcontainer analysis results here]

### Manual Setup (if no devcontainer):
[Existing installation instructions]
```

### 2. Development Environment Setup (DEVCONTAINER-FIRST APPROACH)
**PRIORITIZE containerized development setup:**

```markdown
## 🛠️ Development Environment

### Option 1: DevContainer (Recommended)
**Status**: [✅ Available / ❌ Not configured]

If devcontainer is available:
- **Quick Start**: Open in VS Code → "Reopen in Container"
- **Includes**: ARM toolchain, debugger, code analysis, extensions
- **Configuration**: [Extract from .devcontainer/devcontainer.json]

### Option 2: Manual Installation
[Preserve existing manual setup instructions]
[Update any outdated package versions or procedures]
[Remove steps that are no longer needed]
```

### 3. Software Features (IMPLEMENTED FEATURES ONLY)
**DOCUMENT ONLY features with actual code implementation:**

```markdown
## ✨ Features (Current Implementation)

[Scan codebase and document ONLY implemented features]

### Implemented Protocols
[List ONLY protocols with actual handler code]
- **UART**: [Extract baud rates, frame format from code]
- **SPI**: [Only if SPI driver code exists]
- **I2C**: [Only if I2C implementation found]

### System Commands (Current Parser Implementation)
[Extract command table from actual parser code]
| Command | Syntax | Response | Implementation |
|---------|--------|----------|----------------|
[Include ONLY commands in current command table]

### API Functions (Current Public Interface)
[Include ONLY functions in current header files]
[Use Doxygen comments for descriptions]
[Remove documentation for deleted functions]
```

### 4. Usage and Command Interface (PARSER-SYNCHRONIZED)
**SYNCHRONIZE with actual command parser implementation:**

```markdown
## 📘 Usage Guide

### Command Interface (Synchronized with Parser v[VERSION])
**Status**: [Verified against src/command_parser.c on [DATE]]

#### Available Commands:
[Generate table from actual command parser code]
```c
// Command syntax extracted from parser implementation
typedef struct {
    char* command;
    char* parameters;
    char* description;
    cmd_handler_t handler;
} command_entry_t;
```

[Document ONLY commands that exist in current parser table]
[Remove commands that are no longer handled]
[Update syntax to match current implementation]
```

### 5. API Reference (CODE-EXTRACTED DOCUMENTATION)
**GENERATE from actual header files and Doxygen comments:**

```markdown
## 📚 API Reference (Auto-Generated from Code)

**Last Updated**: [EXTRACTION DATE]
**Source**: [List header files analyzed]

[For each public function in headers:]
### `function_name()`
**Signature**: [Extract exact signature from header]
**Description**: [Use @brief from Doxygen comment]
**Parameters**: [Extract @param documentation]
**Returns**: [Use @return documentation]
**Example**: [Include @code blocks if present]

[Include ONLY functions that currently exist]
[Remove all documentation for deleted functions]
```

### 6. Configuration Management (PARAMETER-SYNCHRONIZED)
**SYNCHRONIZE with actual configuration parameters in code:**

```markdown
## ⚙️ Configuration

### System Parameters (Code-Synchronized)
**Source**: [List config header files]
**Defaults**: [Extract from #define statements]

[Generate table from actual #define CONFIG_* parameters]
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
[Include ONLY parameters that exist in current code]
[Update default values to match code]
[Remove deprecated configuration options]

### Configuration Storage
[Document ONLY if config storage code exists]
[Describe actual implementation, not planned features]
```

### 7. Hardware Platform (IMPLEMENTATION-SPECIFIC)
**DOCUMENT hardware interfaces actually used by code:**

```markdown
## 🔧 Hardware Platform

### Supported Hardware (Code-Verified)
[List ONLY hardware platforms with actual support code]
[Remove hardware references no longer supported]

### Pin Assignments (Current Implementation)
[Extract from actual GPIO initialization code]
[Document ONLY pins actually configured in code]
[Update any changed pin assignments]

### Peripheral Usage (Implementation-Based)
[Document ONLY peripherals initialized in code]
[Remove references to unused peripherals]
```

### 8. Memory and Performance (MEASURED SPECIFICATIONS)
**INCLUDE actual measurements and remove outdated specifications:**

```markdown
## 📊 Memory and Performance

### Memory Usage (Current Build)
[Include ONLY if memory measurement code exists]
[Update with actual measurements from current build]
[Remove outdated memory specifications]

### Performance Metrics (Measured)
[Include ONLY if performance monitoring is implemented]
[Update with actual measurements from current implementation]
[Remove theoretical or outdated performance claims]
```

### 9. Testing and Quality (IMPLEMENTED TESTING ONLY)
**INCLUDE this section ONLY if test code exists in the project:**

```markdown
## 🧪 Testing (If tests exist in codebase)

**Condition**: Include this section ONLY if test/ directory or test files found

### Test Framework (Current Implementation)
[Document ONLY the testing framework actually in use]
[Remove references to planned but unimplemented testing]

### Running Tests (Actual Commands)
[Include ONLY test commands that work with current codebase]
[Update any changed test procedures]
[Remove obsolete test instructions]
```

### 10. Troubleshooting (CURRENT ISSUES AND SOLUTIONS)
**MAINTAIN current troubleshooting info and add new issues:**

```markdown
## 🔧 Troubleshooting

### Known Issues (Current)
[Update list based on current issue tracker]
[Remove resolved issues that are no longer relevant]
[Add new issues discovered in current implementation]

### Common Problems (Implementation-Specific)
[Focus on issues with current codebase]
[Update solutions for current implementation]
[Remove troubleshooting for deprecated features]
```

---

## Final Enhancement Instructions

### ✅ SYNCHRONIZATION CHECKLIST

**Before submitting enhanced README, verify:**

#### Content Accuracy:
- [ ] **All documented functions exist** in current codebase
- [ ] **Function signatures match** actual header file declarations
- [ ] **Command syntax matches** current parser implementation
- [ ] **Configuration parameters match** actual #define values
- [ ] **Protocol documentation** reflects only implemented protocols
- [ ] **Hardware specifications** match actual pin configurations
- [ ] **Build instructions** work with current build system
- [ ] **Examples compile** and work with current codebase

#### Content Removal:
- [ ] **Deprecated functions removed** from API documentation
- [ ] **Obsolete commands removed** from command reference
- [ ] **Outdated protocols removed** from feature list
- [ ] **Removed configuration options** deleted from config section
- [ ] **Discontinued hardware support** removed from hardware section
- [ ] **Broken examples** deleted or fixed
- [ ] **Dead links** removed or updated
- [ ] **Placeholder content** replaced with actual implementation details

#### Enhancement Quality:
- [ ] **Devcontainer setup** documented if configuration exists
- [ ] **Doxygen comments** integrated into API documentation
- [ ] **Code-extracted information** used for accuracy
- [ ] **Implementation details** documented based on actual code
- [ ] **Current version information** throughout documentation
- [ ] **Professional formatting** with consistent structure
- [ ] **Embedded system focus** maintained throughout

### 🎯 OUTPUT REQUIREMENTS

**Generate a complete README.md that:**
1. **Preserves all valid existing content** exactly as written
2. **Removes all outdated and incorrect information** based on code analysis
3. **Adds missing documentation** for newly implemented features
4. **Synchronizes all technical details** with current codebase
5. **Prioritizes devcontainer setup** if configuration exists
6. **Includes only relevant sections** based on actual implementation
7. **Maintains professional embedded C documentation standards**
8. **Provides maintenance tools** for ongoing synchronization

### 🚨 CRITICAL SUCCESS CRITERIA

**The enhanced README must:**
- ✅ **Accurately reflect current codebase** - no documentation without implementation
- ✅ **Include all public APIs** - every function in headers documented correctly
- ✅ **Match command parser exactly** - command reference synchronized with code
- ✅ **Use current configuration values** - parameters match code defaults
- ✅ **Remove all obsolete content** - no references to removed features
- ✅ **Integrate code comments** - Doxygen documentation enhanced README
- ✅ **Prioritize easy setup** - devcontainer instructions if available
- ✅ **Enable ongoing maintenance** - tools provided for continuous synchronization

**Remember: This is LIVING DOCUMENTATION that must stay synchronized with the evolving codebase. Document what IS, remove what WAS, and provide tools to maintain accuracy over time.**