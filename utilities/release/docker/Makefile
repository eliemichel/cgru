PLATFORMS	= darwin-x64 linux-x64 windows-x64
DOCKER		= docker
IMAGE		= build-cgru
.PHONY		= all

default: all

base: Dockerfile
	$(DOCKER) build -t $(IMAGE):base . && touch base

all: base
	for i in $(PLATFORMS); do \
		$(DOCKER) build -t $(IMAGE):$$i $$i ; \
	done
