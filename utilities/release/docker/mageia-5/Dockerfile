FROM mageia:5

RUN urpmi.update -a
RUN urpmi --auto \
	gcc-c++ \
	python \
	python-devel \ 
	git \
	cmake \
	inkscape \
	postgresql-devel \
	rpm-build
RUN urpmi --auto \
	lib64OpenEXR-devel \
	qt4-devel-private 

COPY . /opt/cgru
WORKDIR /opt/cgru

RUN cd utilities/exrjoin && ./build.sh
RUN cd afanasy/src/project.cmake && ./build.sh
RUN cd utilities/release && ./make_linuxpackages.sh --skipcheck