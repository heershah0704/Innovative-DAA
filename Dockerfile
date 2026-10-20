# Use a slim Debian image with g++ available
FROM debian:bookworm-slim

# Install g++ and make
RUN apt-get update && apt-get install -y g++ make && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy all source files
COPY . .

# Compile the C++ server
RUN g++ -std=c++17 -O2 -Wall -Wextra -o server backend/src/main.cpp

# Expose port (Render overrides this with $PORT at runtime)
EXPOSE 8080

# Start the server (reads $PORT env var automatically via our main.cpp fix)
CMD ["./server"]
