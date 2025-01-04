# Install https://courses.grainger.illinois.edu/cs438/fa2024/files/ece438_docker.tar
FROM ece438:latest

# Install the necessary packages
RUN apt-get update
RUN apt-get install -y git ssh

# Copy the repository into the container
COPY . /mp0