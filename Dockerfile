FROM gns3/ubuntu:focal

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update -y \
&& apt-get -y install gcc \
gdb \
build-essential \
tcl-dev \
traceroute \
iputils-ping \
iproute2 \
&& rm -rf /var/lib/apt/lists/*

COPY OSPF_John_Moy_Base/ /root/ospf/.

WORKDIR /root/ospf/linux
RUN make