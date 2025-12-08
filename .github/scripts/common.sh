#!/bin/bash

# ========== COLOR FUNCTIONS ==========
color_red() { echo -e "\033[31m$1\033[0m"; }
color_green() { echo -e "\033[32m$1\033[0m"; }
color_yellow() { echo -e "\033[33m$1\033[0m"; }
color_blue() { echo -e "\033[34m$1\033[0m"; }

# ========== LOGGING FUNCTIONS ==========
log_info() {
    color_blue "[INFO] $1"
}

log_success() {
    color_green "[SUCCESS] $1"
}

log_warning() {
    color_yellow "[WARNING] $1"
}

log_error() {
    color_red "[ERROR] $1"
}

# ========== VALIDATION FUNCTIONS ==========
validate_file_exists() {
    if [ ! -f "$1" ]; then
        log_error "File not found: $1"
        return 1
    fi
    log_info "File exists: $1"
}

validate_directory_exists() {
    if [ ! -d "$1" ]; then
        log_error "Directory not found: $1"
        return 1
    fi
    log_info "Directory exists: $1"
}

# ========== DOCKER HELPER FUNCTIONS ==========
docker_run_with_cleanup() {
    local CONTAINER_NAME="$1"
    local IMAGE="$2"
    shift 2
    local DOCKER_ARGS=("$@")
    
    # Chạy container
    docker run -d --name "$CONTAINER_NAME" "${DOCKER_ARGS[@]}" "$IMAGE" tail -f /dev/null
    
    # Đăng ký cleanup
    trap "docker rm -f '$CONTAINER_NAME' 2>/dev/null || true" EXIT
}

# ========== VERSION FUNCTIONS ==========
get_version_from_tag() {
    local REF="$1"
    if [[ "$REF" == refs/tags/* ]]; then
        echo "${REF#refs/tags/v}"
    else
        echo "0.0.0-dev"
    fi
}

# ========== PACKAGE FUNCTIONS ==========
extract_package_info() {
    local PACKAGE_FILE="$1"
    local INFO
    
    # Lấy thông tin từ tên file
    BASENAME=$(basename "$PACKAGE_FILE" .deb)
    
    # Parse: psensor-fork_1.2.3_ubuntu24.04_amd64.deb
    if [[ $BASENAME =~ psensor-fork_([^_]+)_([^_]+)_([^_]+) ]]; then
        VERSION="${BASH_REMATCH[1]}"
        DISTRO_INFO="${BASH_REMATCH[2]}"
        ARCH="${BASH_REMATCH[3]}"
        
        # Extract distro và version từ DISTRO_INFO
        if [[ $DISTRO_INFO =~ ([a-z]+)([0-9.]+) ]]; then
            DISTRO="${BASH_REMATCH[1]}"
            DISTRO_VERSION="${BASH_REMATCH[2]}"
        fi
        
        echo "$VERSION $DISTRO $DISTRO_VERSION $ARCH"
    fi
}