FROM debian:bookworm-slim AS builder

# Install minimal build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc \
    make \
    libpq-dev \
    libc6-dev \
    && rm -rf /var/lib/apt/lists/*

# Create app directory
WORKDIR /app

# Copy source code, Makefile, and existing configs
COPY src/ ./src/
COPY Makefile .
COPY configs/ ./configs/

# Build the application
RUN mkdir -p build && \
    make

# Final stage
FROM debian:bookworm-slim

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libpq5 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy configs and built binary from builder stage
COPY --from=builder /app/configs /app/configs
COPY --from=builder /app/build/SensorDHS /app/build/SensorDHS

# Command to run the application
CMD ["./build/SensorDHS"]
