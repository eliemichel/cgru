FROM debian:wheezy

RUN DEBIAN_FRONTEND=noninteractive apt-get -qq update \
 && apt-get -yqq install \
	bash \
	curl wget \
	pkg-config build-essential make automake autogen \
	tar xz-utils bzip2 gzip \
	file \
	rsync \
	sed \
	git \ 
	vim

ENV QT_MAJOR_VERSION="4.8" QT_VERSION="4.8.5" \
 OPENEXR_VERSION="2.2.0" \
 FAAC_VERSION="1.28" LAME_MAJOR_VERSION="3.99" LAME_MINOR_VERSION="5" LIBOGG_VERSION="1.3.1" LIBVORBIS_VERSION="1.3.4" LIBTHEORA_VERSION="1.1.1" YASM_VERSION="1.2.0" FFMPEG_VERSION="2.8.6"

WORKDIR /tmp

ADD http://download.qt.io/archive/qt/$QT_MAJOR_VERSION/$QT_VERSION/qt-everywhere-opensource-src-$QT_VERSION.tar.gz ./

ADD http://download.savannah.nongnu.org/releases/openexr/openexr-$OPENEXR_VERSION.tar.gz ./
ADD http://download.savannah.nongnu.org/releases/openexr/ilmbase-$OPENEXR_VERSION.tar.gz ./

ADD http://www.imagemagick.org/download/ImageMagick.tar.gz ./

ADD http://downloads.sourceforge.net/project/faac/faac-src/faac-$FAAC_VERSION/faac-$FAAC_VERSION.tar.bz2 ./
ADD http://downloads.sourceforge.net/project/lame/lame/$LAME_MAJOR_VERSION/lame-$LAME_MAJOR_VERSION.$LAME_MINOR_VERSION.tar.gz ./
ADD http://downloads.xiph.org/releases/ogg/libogg-$LIBOGG_VERSION.tar.gz ./
ADD http://downloads.xiph.org/releases/vorbis/libvorbis-$LIBVORBIS_VERSION.tar.gz ./
ADD http://downloads.xiph.org/releases/theora/libtheora-$LIBTHEORA_VERSION.tar.gz ./
ADD http://www.tortall.net/projects/yasm/releases/yasm-$YASM_VERSION.tar.gz ./
RUN git clone git://git.videolan.org/x264.git
ADD https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.bz2 ./

RUN for i in \
	qt-everywhere-opensource-src-$QT_VERSION.tar.gz \
	openexr-$OPENEXR_VERSION.tar.gz \
	ilmbase-$OPENEXR_VERSION.tar.gz \
	ImageMagick.tar.gz \
	lame-$LAME_MAJOR_VERSION.$LAME_MINOR_VERSION.tar.gz \
	libogg-$LIBOGG_VERSION.tar.gz \
	libvorbis-$LIBVORBIS_VERSION.tar.gz \
	libtheora-$LIBTHEORA_VERSION.tar.gz \
	yasm-$YASM_VERSION.tar.gz \
 ; do tar -xzf $i && rm $i; done \
 && for i in \
 	faac-$FAAC_VERSION.tar.bz2 \
 	ffmpeg-$FFMPEG_VERSION.tar.bz2 \
 ; do tar -xjf $i && rm $i; done
