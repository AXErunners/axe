FROM debian:stretch
LABEL maintainer="AXE Developers <info@axerunners.com>"
LABEL description="Dockerized AXE Core, built from Travis"

RUN apt-get update && apt-get -y upgrade && apt-get clean && rm -fr /var/cache/apt/*

COPY bin/* /usr/bin/
