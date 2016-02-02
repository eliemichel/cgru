FROM centos:7

RUN yum -y update \
 && yum -y install \
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
 && yum clean all

COPY . /opt/cgru
WORKDIR /opt/cgru

RUN cd utilities/exrjoin && ./build.sh
RUN cd afanasy/src/project.cmake && ./build.sh
RUN cd utilities/release && ./make_linuxpackages.sh --skipcheck