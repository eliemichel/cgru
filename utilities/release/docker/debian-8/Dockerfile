FROM debian:8

RUN DEBIAN_FRONTEND=noninteractive apt-get -qq update \
 && apt-get -yqq install \
	g++ \
	python \
	python-dev \
	git \
	cmake \
	inkscape \
	libopenexr-dev \
	libqt4-dev \
	libpqxx3-dev \
 && apt-get -yqq clean \
 && rm -rf /var/lib/apt/lists/*

COPY . /opt/cgru
WORKDIR /opt/cgru

RUN cd utilities/exrjoin && ./build.sh
RUN cd afanasy/src/project.cmake && ./build.sh
RUN cd utilities/release && ./make_linuxpackages.sh --skipcheck