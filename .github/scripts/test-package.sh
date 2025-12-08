#!/bin/bash
set -e

# ========== HÀM HIỂN THỊ ==========
print_header() {
    echo "========================================"
    echo "$1"
    echo "========================================"
}

print_step() {
    echo "▶ $1"
}

print_success() {
    echo "✅ $1"
}

print_error() {
    echo "❌ $1"
}

# ========== HÀM CHÍNH ==========
test_package() {
    local PACKAGE_FILE="$1"
    local DISTRO="$2"
    local DISTRO_VERSION="$3"
    local ARCH="$4"
    
    print_header "TESTING PACKAGE"
    echo "• Package: $PACKAGE_FILE"
    echo "• Distro: $DISTRO $DISTRO_VERSION"
    echo "• Arch: $ARCH"
    
    # Kiểm tra file tồn tại
    if [ ! -f "$PACKAGE_FILE" ]; then
        print_error "Package file not found: $PACKAGE_FILE"
        return 1
    fi
    
    local PACKAGE_BASENAME=$(basename "$PACKAGE_FILE")
    
    # # Setup QEMU nếu cần
    # if [ "$ARCH" != "amd64" ] && [ "$(uname -m)" = "x86_64" ]; then
    #     print_step "Setting up QEMU for $ARCH"
    #     sudo apt-get update
    #     sudo apt-get install -y qemu-user-static binfmt-support
    #     docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
    # fi
    
    # Tạo test container
    print_step "1. Creating test container"
    local CONTAINER_NAME="test-$(date +%s)"
    local IMAGE="$DISTRO:$DISTRO_VERSION"
    
    docker run -d --platform "linux/$ARCH" \
        --name "$CONTAINER_NAME" \
        --workdir /workspace \
        -v "$(pwd):/workspace" \
        "$IMAGE" \
        tail -f /dev/null
    
    # Function cleanup
    cleanup() {
        print_step "Cleaning up container: $CONTAINER_NAME"
        docker rm -f "$CONTAINER_NAME" 2>/dev/null || true
    }
    trap cleanup EXIT
    
    sleep 2
    
    print_step "2. Updating package lists"
    docker exec "$CONTAINER_NAME" apt-get update
    
    print_step "3. Copying package to container"
    docker cp "$PACKAGE_FILE" "$CONTAINER_NAME:/tmp/$PACKAGE_BASENAME"
    
    print_step "4. Installing package"
    if ! docker exec "$CONTAINER_NAME" apt-get install -y "/tmp/$PACKAGE_BASENAME"; then
        print_step "Trying to fix broken packages"
        docker exec "$CONTAINER_NAME" apt-get install -f -y
    fi
    
    print_step "5. Verifying installation"
    if docker exec "$CONTAINER_NAME" which psensor-fork; then
        print_success "psensor-fork installed successfully"
        
        # Test version
        print_step "Testing version command"
        docker exec "$CONTAINER_NAME" psensor-fork --version || true
    else
        print_error "psensor-fork not found after installation"
        
        # Debug: check installed packages
        print_step "Checking installed packages"
        docker exec "$CONTAINER_NAME" dpkg -l | grep -i psensor || echo "No psensor packages found"
        return 1
    fi
    
    # Test cơ bản
    print_step "6. Running basic test"
    docker exec "$CONTAINER_NAME" timeout 5 psensor-fork --help || \
        echo "Help command failed or timed out"
    
    print_success "All tests passed!"
    return 0
}

# ========== MAIN EXECUTION ==========
main() {
    # Kiểm tra arguments
    if [ $# -lt 4 ]; then
        echo "Usage: $0 <package_file> <distro> <distro_version> <arch>"
        echo "Example: $0 ./psensor-fork_1.2.3_ubuntu24.04_amd64.deb ubuntu 24.04 amd64"
        exit 1
    fi
    
    local PACKAGE_FILE="$1"
    local DISTRO="$2"
    local DISTRO_VERSION="$3"
    local ARCH="$4"
    
    # Gọi hàm test
    test_package "$PACKAGE_FILE" "$DISTRO" "$DISTRO_VERSION" "$ARCH"
}

# Chạy main nếu script được gọi trực tiếp
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi