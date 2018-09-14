FROM debian:stretch
LABEL maintainer="AXErunners"
LABEL description="Dockerized AXE Core, built from Travis"

RUN apt-get update && apt-get -y upgrade && apt-get clean && rm -fr /var/cache/apt/*

COPY bin/* /usr/bin/
