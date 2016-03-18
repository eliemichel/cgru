#pragma once

#include "rendercontainer.h"
#include "clientcontainer.h"
#include "renderaf.h"

#include "../libafanasy/msgclasses/mctaskup.h"

/// Renders container.
class RenderContainer : public ClientContainer
{
public:
   RenderContainer();
   ~RenderContainer();

   /// Add new Render to container, new id returned on success, else return 0.
   af::Msg * addRender( RenderAf *newRender, MonitorContainer * monitoring = NULL);

   /// Inherited from MsgHandlerItf
   virtual bool processMsg(af::Msg *msg);
};

/// Renders iterator.
class RenderContainerIt : public AfContainerIt
{
public:
   RenderContainerIt( RenderContainer* container, bool skipZombies = true);
   ~RenderContainerIt();

	inline RenderAf * render() { return (RenderAf*)(getNode()); }
	inline RenderAf * getRender( int id) { return (RenderAf*)(get( id)); }

private:
};
