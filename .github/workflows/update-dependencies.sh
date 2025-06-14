#!/bin/bash
set -euo pipefail

# Configuration variables with defaults
DEPENDENCIES_DIR="${DEPENDENCIES_DIR:-.devcontainer}"
DEPENDENCIES_FILENAME="${DEPENDENCIES_FILENAME:-dependencies.json}"
DEPENDENCIES_FILE="${DEPENDENCIES_FILE:-$DEPENDENCIES_DIR/$DEPENDENCIES_FILENAME}"
OUTPUT_FILE="${OUTPUT_FILE:-$DEPENDENCIES_FILE}"
DRY_RUN="${DRY_RUN:-false}"
VERBOSE="${VERBOSE:-false}"
NO_BACKUP="${NO_BACKUP:-false}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" >&2
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

log_debug() {
    if [[ "$VERBOSE" == "true" ]]; then
        echo -e "${BLUE}[DEBUG]${NC} $1" >&2
    fi
}

# Print usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Update package versions in dependencies JSON file located in .devcontainer folder.

Environment Variables:
  DEPENDENCIES_DIR      Directory containing the dependencies file (default: .devcontainer)
  DEPENDENCIES_FILENAME Name of the dependencies file (default: dependencies.json)
  DEPENDENCIES_FILE     Full path to dependencies file (overrides DIR + FILENAME)
  OUTPUT_FILE          Output JSON file (default: same as input)
  DRY_RUN              Show changes without writing (default: false)
  VERBOSE              Enable verbose logging (default: false)
  NO_BACKUP            Skip creating backup files (default: false)

Examples:
  $0                                                    # Update .devcontainer/dependencies.json
  DEPENDENCIES_DIR="config" $0                         # Use config/dependencies.json
  DEPENDENCIES_FILENAME="packages.json" $0             # Use .devcontainer/packages.json
  DEPENDENCIES_FILE="custom/deps.json" $0              # Use exact path
  OUTPUT_FILE=".devcontainer/new-deps.json" $0         # Write to different file
  DRY_RUN=true $0                                      # Show changes without writing
  VERBOSE=true $0                                      # Enable verbose output
  NO_BACKUP=true $0                                    # Skip backup creation

Command Line Options:
  -d, --dir DIR         Set dependencies directory
  -f, --file FILE       Set full path to dependencies file
  -n, --name NAME       Set dependencies filename
  -o, --output FILE     Set output file
  --dry-run            Show changes without writing
  -v, --verbose        Enable verbose logging
  --no-backup          Skip creating backup files
  -h, --help           Show this help message

EOF
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
            exit 0
            ;;
        -d|--dir)
            DEPENDENCIES_DIR="$2"
            DEPENDENCIES_FILE="$DEPENDENCIES_DIR/$DEPENDENCIES_FILENAME"
            shift 2
            ;;
        -f|--file)
            DEPENDENCIES_FILE="$2"
            shift 2
            ;;
        -n|--name)
            DEPENDENCIES_FILENAME="$2"
            DEPENDENCIES_FILE="$DEPENDENCIES_DIR/$DEPENDENCIES_FILENAME"
            shift 2
            ;;
        -o|--output)
            OUTPUT_FILE="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        --no-backup)
            NO_BACKUP=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            exit 1
            ;;
    esac
done

# Resolve relative paths to absolute paths
DEPENDENCIES_FILE=$(realpath "$DEPENDENCIES_FILE" 2>/dev/null || echo "$DEPENDENCIES_FILE")
OUTPUT_FILE=$(realpath "$OUTPUT_FILE" 2>/dev/null || echo "$OUTPUT_FILE")

# Check if dependencies directory exists
DEPENDENCIES_DIR_ACTUAL=$(dirname "$DEPENDENCIES_FILE")
if [[ ! -d "$DEPENDENCIES_DIR_ACTUAL" ]]; then
    log_error "Dependencies directory does not exist: $DEPENDENCIES_DIR_ACTUAL"
    log_info "You may need to create it first: mkdir -p $DEPENDENCIES_DIR_ACTUAL"
    exit 1
fi

# Check if dependencies file exists
if [[ ! -f "$DEPENDENCIES_FILE" ]]; then
    log_error "Dependencies file not found: $DEPENDENCIES_FILE"
    log_info "Expected location: $DEPENDENCIES_FILE"
    exit 1
fi

# Validate JSON format
if ! python3 -m json.tool "$DEPENDENCIES_FILE" > /dev/null 2>&1; then
    log_error "$DEPENDENCIES_FILE is not valid JSON"
    exit 1
fi

log_info "Starting dependency updates"
log_debug "Dependencies directory: $DEPENDENCIES_DIR_ACTUAL"
log_debug "Input file: $DEPENDENCIES_FILE"
log_debug "Output file: $OUTPUT_FILE"
log_debug "Dry run: $DRY_RUN"

JSON=$(cat "$DEPENDENCIES_FILE")
updated_count=0
error_count=0
changes_made=false

for PACKAGE in $(echo "$JSON" | jq -r 'keys | .[]'); do
    log_debug "Processing package: $PACKAGE"
    
    if VERSION=$(apt-cache policy "$PACKAGE" 2>/dev/null | grep -oP '(?<=Candidate:\s)(.+)'); then
        CURRENT_VERSION=$(echo "$JSON" | jq -r --arg package "$PACKAGE" '.[$package] // ""')
        
        if [[ -n "$CURRENT_VERSION" && "$CURRENT_VERSION" != "null" ]]; then
            # Compare versions using dpkg --compare-versions
            if dpkg --compare-versions "$VERSION" gt "$CURRENT_VERSION" 2>/dev/null; then
                log_info "Updating $PACKAGE: $CURRENT_VERSION -> $VERSION"
                # Always update JSON for consistency, regardless of dry run
                NEW_JSON=$(echo "$JSON" | jq '.[$package] = $version' --arg package "$PACKAGE" --arg version "$VERSION" 2>&1)
                if [[ $? -eq 0 ]]; then
                    JSON="$NEW_JSON"
                    log_debug "Successfully updated JSON for package: $PACKAGE"
                else
                    log_error "Failed to update JSON for package: $PACKAGE"
                    log_error "jq error: $NEW_JSON"
                    exit 1
                fi
                
                # Increment counter safely
                updated_count=$((updated_count + 1))
                log_debug "Updated count incremented to: $updated_count"
                
                changes_made=true
                log_debug "Changes flag set to true for package: $PACKAGE"
            else
                log_debug "Keeping $PACKAGE at $CURRENT_VERSION (candidate: $VERSION)"
            fi
        else
            # No current version, add the new one
            log_info "Adding $PACKAGE: $VERSION"
            # Always update JSON for consistency, regardless of dry run
            NEW_JSON=$(echo "$JSON" | jq '.[$package] = $version' --arg package "$PACKAGE" --arg version "$VERSION" 2>&1)
            if [[ $? -eq 0 ]]; then
                JSON="$NEW_JSON"
                log_debug "Successfully updated JSON for package: $PACKAGE"
            else
                log_error "Failed to update JSON for package: $PACKAGE"
                log_error "jq error: $NEW_JSON"
                exit 1
            fi
            
            # Increment counter safely
            updated_count=$((updated_count + 1))
            log_debug "Updated count incremented to: $updated_count"
            
            changes_made=true
            log_debug "Changes flag set to true for new package: $PACKAGE"
        fi
    else
        log_warn "Could not get version for package: $PACKAGE"
        error_count=$((error_count + 1))
    fi
    
    log_debug "Finished processing package: $PACKAGE"
done

log_debug "Processing completed. Changes made: $changes_made, Updated count: $updated_count"

# Write updated JSON back to file (unless dry run)
if [[ "$DRY_RUN" != "true" ]]; then
    if [[ "$changes_made" == "true" ]]; then
        log_debug "Changes detected, proceeding with file update"
        
        # Debug: Show current working directory and file permissions
        log_debug "Current working directory: $(pwd)"
        log_debug "Output file permissions before update: $(ls -la "$OUTPUT_FILE" 2>/dev/null || echo "File not found")"
        log_debug "Output directory permissions: $(ls -ld "$(dirname "$OUTPUT_FILE")" 2>/dev/null || echo "Directory not found")"
        
        # Ensure output directory exists
        OUTPUT_DIR=$(dirname "$OUTPUT_FILE")
        if [[ ! -d "$OUTPUT_DIR" ]]; then
            log_info "Creating output directory: $OUTPUT_DIR"
            mkdir -p "$OUTPUT_DIR"
        fi
        
        # Check if we can write to the output directory
        if [[ ! -w "$OUTPUT_DIR" ]]; then
            log_error "No write permission to output directory: $OUTPUT_DIR"
            exit 1
        fi
        
        TEMP_FILE="${OUTPUT_FILE}.tmp"
        log_debug "Writing updated JSON to temporary file: $TEMP_FILE"
        
        # Debug: Show the JSON content that will be written
        log_debug "JSON content preview (first 200 chars): $(echo "$JSON" | head -c 200)..."
        
        if ! echo "$JSON" | python3 -m json.tool > "$TEMP_FILE"; then
            log_error "Failed to format JSON output"
            rm -f "$TEMP_FILE"
            exit 1
        fi
        
        # Verify temp file was created and has content
        if [[ ! -f "$TEMP_FILE" ]]; then
            log_error "Temporary file was not created: $TEMP_FILE"
            exit 1
        fi
        
        TEMP_SIZE=$(stat -c%s "$TEMP_FILE" 2>/dev/null || echo "0")
        log_debug "Temporary file created with size: $TEMP_SIZE bytes"
        
        if [[ "$TEMP_SIZE" -eq 0 ]]; then
            log_error "Temporary file is empty"
            rm -f "$TEMP_FILE"
            exit 1
        fi
        
        # Check if output file exists and backup if needed
        if [[ -f "$OUTPUT_FILE" && "$NO_BACKUP" != "true" ]]; then
            BACKUP_FILE="${OUTPUT_FILE}.backup.$(date +%s)"
            log_debug "Creating backup: $BACKUP_FILE"
            cp "$OUTPUT_FILE" "$BACKUP_FILE"
        elif [[ -f "$OUTPUT_FILE" && "$NO_BACKUP" == "true" ]]; then
            log_debug "Skipping backup creation (NO_BACKUP=true)"
        fi
        
        log_debug "Moving temporary file to output file: $OUTPUT_FILE"
        if ! mv "$TEMP_FILE" "$OUTPUT_FILE"; then
            log_error "Failed to move temporary file to output file"
            log_error "Error details: $(mv "$TEMP_FILE" "$OUTPUT_FILE" 2>&1 || echo "Move operation failed")"
            rm -f "$TEMP_FILE"
            exit 1
        fi
        
        log_info "File updated: $OUTPUT_FILE"
        
        # Verify the file was written correctly
        if [[ -f "$OUTPUT_FILE" ]]; then
            NEW_SIZE=$(stat -c%s "$OUTPUT_FILE" 2>/dev/null || echo "0")
            log_debug "Output file exists with size: $NEW_SIZE bytes"
            log_debug "Output file permissions after update: $(ls -la "$OUTPUT_FILE")"
            
            if python3 -m json.tool "$OUTPUT_FILE" > /dev/null 2>&1; then
                log_debug "Output file contains valid JSON"
                
                # Show a preview of the updated content
                log_debug "Updated file preview: $(head -n 5 "$OUTPUT_FILE")"
            else
                log_error "Output file does not contain valid JSON"
                exit 1
            fi
        else
            log_error "Output file was not created successfully"
            exit 1
        fi
    else
        log_info "No changes were made, file not updated"
    fi
else
    log_info "Dry run completed - no files were modified"
fi

log_info "Dependency update completed!"
if [[ "$DRY_RUN" == "true" ]]; then
    log_info "Packages that would be updated: $updated_count"
else
    log_info "Packages updated: $updated_count"
fi
if [[ $error_count -gt 0 ]]; then
    log_warn "Packages with errors: $error_count"
fi

# Show final status
if [[ "$changes_made" == "true" && "$DRY_RUN" != "true" ]]; then
    log_info "✓ Dependencies file has been updated successfully"
elif [[ "$changes_made" == "false" ]]; then
    log_info "✓ All packages are already at their latest versions"
fi

exit 0