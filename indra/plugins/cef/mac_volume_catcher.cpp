/** 
 * @file mac_volume_catcher.cpp
 * @brief A Mac OS X specific hack to control the volume level of all audio channels opened by a process.
 *
 * @cond
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 * @endcond
 */

/**************************************************************************************************************
	This code works by using CaptureComponent to capture the "Default Output" audio component
	(kAudioUnitType_Output/kAudioUnitSubType_DefaultOutput) and delegating all calls to the original component.
	It does this just to keep track of all instances of the default output component, so that it can set the
	kHALOutputParam_Volume parameter on all of them to adjust the output volume.
**************************************************************************************************************/

#include "volume_catcher.h"

#include <QuickTime/QuickTime.h>
#include <AudioUnit/AudioUnit.h>
#include <list>

#if LL_DARWIN
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

struct VolumeCatcherStorage;

class VolumeCatcherImpl : public LLSingleton<VolumeCatcherImpl>
{
	friend LLSingleton<VolumeCatcherImpl>;
	VolumeCatcherImpl();
	// This is a singleton class -- both callers and the component implementation should use getInstance() to find the instance.
	~VolumeCatcherImpl();

public:

	void setVolume(F32 volume);
	void setPan(F32 pan);
	
	std::list<VolumeCatcherStorage*> mComponentInstances;
	Component mOriginalDefaultOutput;
	Component mVolumeAdjuster;

private:
	F32 mVolume;
	F32 mPan;
};

struct VolumeCatcherStorage
{
	ComponentInstance self, delegate;
};

static ComponentResult volume_catcher_component_entry(ComponentParameters *cp, Handle componentStorage);
static ComponentResult volume_catcher_component_open(VolumeCatcherStorage *storage, ComponentInstance self);
static ComponentResult volume_catcher_component_close(VolumeCatcherStorage *storage, ComponentInstance self);

VolumeCatcherImpl::VolumeCatcherImpl()
:	mVolume(1.0f),			// default volume is max
	mPan(0.f)				// default pan is centered
{
	ComponentDescription desc;
	desc.componentType = kAudioUnitType_Output;
	desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	// Find the original default output component
	mOriginalDefaultOutput = FindNextComponent(NULL, &desc);

	// Register our own output component with the same parameters
	mVolumeAdjuster = RegisterComponent(&desc, NewComponentRoutineUPP(volume_catcher_component_entry), 0, NULL, NULL, NULL);

	// Capture the original component, so we always get found instead.
	CaptureComponent(mOriginalDefaultOutput, mVolumeAdjuster);
}

static ComponentResult volume_catcher_component_entry(ComponentParameters *cp, Handle componentStorage)
{
	ComponentResult result = badComponentSelector;
	VolumeCatcherStorage *storage = (VolumeCatcherStorage*)componentStorage;
	
	switch(cp->what)
	{
		case kComponentOpenSelect:
//			std::cerr << "kComponentOpenSelect" << std::endl;
			result = CallComponentFunctionWithStorageProcInfo((Handle)storage, cp, (ProcPtr)volume_catcher_component_open, uppCallComponentOpenProcInfo);
		break;

		case kComponentCloseSelect:
//			std::cerr << "kComponentCloseSelect" << std::endl;
			result = CallComponentFunctionWithStorageProcInfo((Handle)storage, cp, (ProcPtr)volume_catcher_component_close, uppCallComponentCloseProcInfo);
			// CallComponentFunctionWithStorageProcInfo
		break;
		
		default:
//			std::cerr << "Delegating selector: " << cp->what << " to component instance " << storage->delegate << std::endl;
			result = DelegateComponentCall(cp, storage->delegate);
		break;
	}
	
	return result;
}

static ComponentResult volume_catcher_component_open(VolumeCatcherStorage *storage, ComponentInstance self)
{
	VolumeCatcherImpl *impl = VolumeCatcherImpl::getInstance();	
	
	storage = new VolumeCatcherStorage;

	storage->self = self;
	storage->delegate = nullptr;

	ComponentResult result = OpenAComponent(impl->mOriginalDefaultOutput, &(storage->delegate));
	
	if(result != noErr)
	{
//		std::cerr << "OpenAComponent result = " << result << ", component ref = " << storage->delegate << std::endl;
		// If we failed to open the delagate component, our open is going to fail.  Clean things up.
		delete storage;
	}
	else
	{
		// Success -- set up this component's storage
		SetComponentInstanceStorage(self, (Handle)storage);

		// add this instance to the global list
		impl->mComponentInstances.push_back(storage);	
		
		// and set up the initial volume
		impl->setVolume(storage);
	}

	return result;
}

static ComponentResult volume_catcher_component_close(VolumeCatcherStorage *storage, ComponentInstance self)
{
	if(storage)
	{
		if(storage->delegate)
		{
			CloseComponent(storage->delegate);
			storage->delegate = nullptr;
		}
		
		VolumeCatcherImpl *impl = VolumeCatcherImpl::getInstance();	
		impl->mComponentInstances.remove(storage);
		delete[] storage;
	}
		
	return noErr;
}

void VolumeCatcherImpl::setVolume(F32 volume)
{
	mVolume = volume;

	// Iterate through all known instances, setting the volume on each.
	for(auto& instance : mComponentInstances)
	{
//		std::cerr << "Setting volume on component instance: " << (instance->delegate) << " to " << mVolume << std::endl;
		if (instance && instance->delegate)
		{
			if (OSStatus err = AudioUnitSetParameter(
					instance->delegate,
					kHALOutputParam_Volume,
					kAudioUnitScope_Global,
					0,
					mVolume,
					0))
			{
//				std::cerr << "    AudioUnitSetParameter returned " << err << std::endl;
			}
		}
	}
}

void VolumeCatcherImpl::setPan(F32 pan)
{
	VolumeCatcherImpl *impl = VolumeCatcherImpl::getInstance();	
	impl->mPan = pan;

	// TODO: implement this.
	// This will probably require adding a "panner" audio unit to the chain somehow.
	// There's also a "3d mixer" component that we might be able to use...
}

/////////////////////////////////////////////////////

VolumeCatcher::VolumeCatcher()
{
	pimpl = VolumeCatcherImpl::getInstance();
}

VolumeCatcher::~VolumeCatcher()
{
	// Let the instance persist until exit.
}

void VolumeCatcher::setVolume(F32 volume)
{
	pimpl->setVolume(volume);
}

void VolumeCatcher::setPan(F32 pan)
{
	pimpl->setPan(pan);
}

void VolumeCatcher::pump()
{
	// No periodic tasks are necessary for this implementation.
}

#if LL_DARWIN
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif
