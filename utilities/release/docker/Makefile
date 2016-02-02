DOCKER		= docker
IMAGE		= build-cgru
PLATFORMS	= debian-7 debian-8 ubuntu-14.04 ubuntu-12.04 centos-6 centos-7 fedora-23 mageia-5 opensuse-42.1

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

fedora-23:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

mageia-5:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

opensuse-42.1:
	$(DOCKER) build -t $(IMAGE):$(@F) -f $(@F)/Dockerfile ../../../

base: Dockerfile
	$(DOCKER) build -t $(IMAGE):base . && touch base