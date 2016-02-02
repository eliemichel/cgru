DOCKER		= docker
IMAGE		= build-cgru
PLATFORMS	= debian-7 debian-8 ubuntu-14.04 ubuntu-12.04 centos-6 centos-7

.PHONY: all clean $(PLATFORMS)

default: all

all: $(PLATFORMS)

debian-7:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

debian-8:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

ubuntu-14.04:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

ubuntu-12.04:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

centos-6:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

centos-7:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

base: Dockerfile
	$(DOCKER) build -t $(IMAGE):base . && touch base