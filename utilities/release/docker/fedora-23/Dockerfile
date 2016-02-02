FROM fedora:23

RUN dnf -y update \
 && dnf -y install \
	gcc-c++ \
	python \
	python-devel \ 
	git \
	cmake \
	inkscape \
	openexr-devel \
	qt-devel \
	postgresql-devel \
	make \
	rpm-build \
 && dnf clean all

COPY . /opt/cgru
WORKDIR /opt/cgru

RUN cd utilities/exrjoin && ./build.sh
RUN cd afanasy/src/project.cmake && ./build.sh
RUN cd utilities/release && ./make_linuxpackages.sh --skipcheck