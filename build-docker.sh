#!/bin/bash

# Build Docker image for metamod-vconsole
# Usage: ./build-docker.sh

set -e

IMAGE_NAME="metamod-vconsole"
IMAGE_TAG="latest"

echo "Building Docker image: ${IMAGE_NAME}:${IMAGE_TAG}"

# Get current user's UID and GID
USER_ID=$(id -u)
GROUP_ID=$(id -g)

docker build \
    --build-arg USER_ID="${USER_ID}" \
    --build-arg GROUP_ID="${GROUP_ID}" \
    -t "${IMAGE_NAME}:${IMAGE_TAG}" \
    .

echo "Docker image built successfully: ${IMAGE_NAME}:${IMAGE_TAG}"
