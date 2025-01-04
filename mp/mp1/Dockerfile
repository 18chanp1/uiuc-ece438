# Install https://courses.grainger.illinois.edu/cs438/fa2024/files/ece438_docker.tar
FROM ece438:latest

# Install the necessary packages
RUN apt-get update -y
RUN apt-get upgrade -y --fix-missing

# Install packages
RUN apt-get install -y gcc make gdb valgrind iperf tcpdump 

# Copy the repository into the container
COPY . /mp1

ENTRYPOINT [ "/bin/bash" ]