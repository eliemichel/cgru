DOCKER		= docker
IMAGE		= build-cgru
PLATFORMS	= debian-7 debian-8
.PHONY		= all clean $(PLATFORMS)

default: all

all: $(PLATFORMS)

debian-7: FORCE
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

debian-8: FORCE
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

ubuntu-14.04: FORCE
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

base: Dockerfile
	$(DOCKER) build -t $(IMAGE):base . && touch base

FORCE: