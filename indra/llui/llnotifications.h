/**
* @file llnotifications.h
* @brief Non-UI manager and support for keeping a prioritized list of notifications
* @author Q (with assistance from Richard and Coco)
*
* $LicenseInfo:firstyear=2008&license=viewergpl$
* 
* Copyright (c) 2008-2009, Linden Research, Inc.
* 
* Second Life Viewer Source Code
* The source code in this file ("Source Code") is provided by Linden Lab
* to you under the terms of the GNU General Public License, version 2.0
* ("GPL"), unless you have obtained a separate licensing agreement
* ("Other License"), formally executed by you and Linden Lab.  Terms of
* the GPL can be found in doc/GPL-license.txt in this distribution, or
* online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
* 
* There are special exceptions to the terms and conditions of the GPL as
* it is applied to this Source Code. View the full text of the exception
* in the file doc/FLOSS-exception.txt in this software distribution, or
* online at
* http://secondlifegrid.net/programs/open_source/licensing/flossexception
* 
* By copying, modifying or distributing this software, you acknowledge
* that you have read and understood your obligations described above,
* and agree to abide by those obligations.
* 
* ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
* WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
* COMPLETENESS OR PERFORMANCE.
* $/LicenseInfo$
*/

#ifndef LL_LLNOTIFICATIONS_H
#define LL_LLNOTIFICATIONS_H

/**
 * This system is intended to provide a singleton mechanism for adding
 * notifications to one of an arbitrary set of event channels.
 * 
 * Controlling JIRA: DEV-9061
 *
 * Every notification has (see code for full list):
 *  - a textual name, which is used to look up its template in the XML files
 *  - a payload, which is a block of LLSD
 *  - a channel, which is normally extracted from the XML files but
 *	  can be overridden.
 *  - a timestamp, used to order the notifications
 *  - expiration time -- if nonzero, specifies a time after which the
 *    notification will no longer be valid.
 *  - a callback name and a couple of status bits related to callbacks (see below)
 * 
 * There is a management class called LLNotifications, which is an LLSingleton.
 * The class maintains a collection of all of the notifications received
 * or processed during this session, and also manages the persistence
 * of those notifications that must be persisted.
 * 
 * We also have Channels. A channel is a view on a collection of notifications;
 * The collection is defined by a filter function that controls which
 * notifications are in the channel, and its ordering is controlled by 
 * a comparator. 
 *
 * There is a hierarchy of channels; notifications flow down from
 * the management class (LLNotifications, which itself inherits from
 * The channel base class) to the individual channels.
 * Any change to notifications (add, delete, modify) is 
 * automatically propagated through the channel hierarchy.
 * 
 * We provide methods for adding a new notification, for removing
 * one, and for managing channels. Channels are relatively cheap to construct
 * and maintain, so in general, human interfaces should use channels to
 * select and manage their lists of notifications.
 * 
 * We also maintain a collection of templates that are loaded from the 
 * XML file of template translations. The system supports substitution
 * of named variables from the payload into the XML file.
 * 
 * By default, only the "unknown message" template is built into the system.
 * It is not an error to add a notification that's not found in the 
 * template system, but it is logged.
 *
 */

#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <iomanip>
#include <sstream>

#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/type_traits.hpp>
#include <boost/signals2.hpp>
#include <boost/range.hpp>
// we want to minimize external dependencies, but this one is important
#include "llsd.h"
#include "llinstancetracker.h"

// and we need this to manage the notification callbacks
#include "llavatarname.h"
#include "llevents.h"
#include "llfunctorregistry.h"
#include "llinitparam.h"
#include "llui.h"
#include "llxmlnode.h"
#include "llnotificationptr.h"
#include "llnotificationcontext.h"
#include "aithreadsafe.h"

namespace AIAlert { class Error; }

typedef enum e_notification_priority
{
	NOTIFICATION_PRIORITY_UNSPECIFIED,
	NOTIFICATION_PRIORITY_LOW,
	NOTIFICATION_PRIORITY_NORMAL,
	NOTIFICATION_PRIORITY_HIGH,
	NOTIFICATION_PRIORITY_CRITICAL
} ENotificationPriority;

typedef boost::function<void (const LLSD&, const LLSD&)> LLNotificationResponder;

typedef LLFunctorRegistry<LLNotificationResponder> LLNotificationFunctorRegistry;
typedef LLFunctorRegistration<LLNotificationResponder> LLNotificationFunctorRegistration;

// Contains notification form data, such as buttons and text fields along with
// manipulator functions
class LLNotificationForm
{
	LOG_CLASS(LLNotificationForm);

public:
	struct FormElementBase : public LLInitParam::Block<FormElementBase>
	{
		Optional<std::string>	name;
		Optional<bool>			enabled;

		FormElementBase();
	};

	struct FormIgnore : public LLInitParam::Block<FormIgnore, FormElementBase>
	{
		Optional<std::string>	text;
		Optional<bool>			save_option;
		Optional<std::string>	control;
		Optional<bool>			invert_control;

		FormIgnore();
	};

	struct FormButton : public LLInitParam::Block<FormButton, FormElementBase>
	{
		Mandatory<S32>			index;
		Mandatory<std::string>	text;
		Optional<std::string>	ignore;
		Optional<bool>			is_default;

		Mandatory<std::string>	type;

		FormButton();
	};

	struct FormInput : public LLInitParam::Block<FormInput, FormElementBase>
	{
		Mandatory<std::string>	type;
		Optional<S32>			width;
		Optional<S32>			max_length_chars;
		Optional<std::string>	text;

		Optional<std::string>	value;
		FormInput();
	};

	struct FormElement : public LLInitParam::ChoiceBlock<FormElement>
	{
		Alternative<FormButton> button;
		Alternative<FormInput>	input;

		FormElement();
	};

	struct FormElements : public LLInitParam::Block<FormElements>
	{
		Multiple<FormElement> elements;
		FormElements();
	};

	struct Params : public LLInitParam::Block<Params>
	{
		Optional<std::string>	name;
		Optional<FormIgnore>	ignore;
		Optional<FormElements>	form_elements;

		Params();
	};

	typedef enum e_ignore_type
	{ 
		IGNORE_NO,
		IGNORE_WITH_DEFAULT_RESPONSE, 
		IGNORE_WITH_LAST_RESPONSE, 
		IGNORE_SHOW_AGAIN 
	} EIgnoreType;

	LLNotificationForm();
	LLNotificationForm(const LLSD& sd);
	LLNotificationForm(const std::string& name, const LLXMLNodePtr xml_node);

	LLSD asLLSD() const;

	S32 getNumElements() { return mFormData.size(); }
	LLSD getElement(S32 index) { return mFormData.get(index); }
	LLSD getElement(const std::string& element_name);
	bool hasElement(const std::string& element_name);
	void addElement(const std::string& type, const std::string& name, const LLSD& value = LLSD());
	void formatElements(const LLSD& substitutions);
	// appends form elements from another form serialized as LLSD
	void append(const LLSD& sub_form);
	std::string getDefaultOption();
	LLPointer<class LLControlVariable> getIgnoreSetting();
	bool getIgnored();
	void setIgnored(bool ignored);

	EIgnoreType getIgnoreType() { return mIgnore; }
	std::string getIgnoreMessage() { return mIgnoreMsg; }

private:
	LLSD	mFormData;
	EIgnoreType mIgnore;
	std::string							mIgnoreMsg;
	LLPointer<class LLControlVariable>	mIgnoreSetting;
	bool								mInvertSetting;
};

typedef boost::shared_ptr<LLNotificationForm> LLNotificationFormPtr;


struct LLNotificationTemplate;

// we want to keep a map of these by name, and it's best to manage them
// with smart pointers
typedef boost::shared_ptr<LLNotificationTemplate> LLNotificationTemplatePtr;

/**
 * @class LLNotification
 * @brief The object that expresses the details of a notification
 * 
 * We make this noncopyable because
 * we want to manage these through LLNotificationPtr, and only
 * ever create one instance of any given notification.
 * 
 * The enable_shared_from_this flag ensures that if we construct
 * a smart pointer from a notification, we'll always get the same
 * shared pointer.
 */
class LLNotification  : 
	boost::noncopyable,
	public boost::enable_shared_from_this<LLNotification>
{
LOG_CLASS(LLNotification);
friend class LLNotifications;
friend struct UpdateItem;

public:
	// parameter object used to instantiate a new notification
	class Params : public LLParamBlock<Params>
	{
		friend class LLNotification;
	public:
		Params(const std::string& _name) 
			:	name(_name),
				mTemporaryResponder(false),
				functor_name(_name),
				priority(NOTIFICATION_PRIORITY_UNSPECIFIED),
				timestamp(LLDate::now())
		{
		}

		// pseudo-param
		Params& functor(LLNotificationFunctorRegistry::ResponseFunctor f) 
		{ 	
			functor_name = LLUUID::generateNewID().asString();
			LLNotificationFunctorRegistry::instance().registerFunctor(functor_name, f);

			mTemporaryResponder = true;
			return *this;
		}

		LLMandatoryParam<std::string>					name;

		// optional
		LLOptionalParam<LLSD>							substitutions;
		LLOptionalParam<LLSD>							payload;
		LLOptionalParam<ENotificationPriority>			priority;
		LLOptionalParam<LLSD>							form_elements;
		LLOptionalParam<LLDate>							timestamp;
		LLOptionalParam<LLNotificationContext*>			context;
		LLOptionalParam<std::string>					functor_name;

	private:
		bool					mTemporaryResponder;
	};

private:
	
	LLUUID mId;
	LLSD mPayload;
	LLSD mSubstitutions;
	LLDate mTimestamp;
	LLDate mExpiresAt;
	bool mCancelled;
	bool mRespondedTo; 	// once the notification has been responded to, this becomes true
	bool mIgnored;
	ENotificationPriority mPriority;
	LLNotificationFormPtr mForm;
	
	// a reference to the template
	LLNotificationTemplatePtr mTemplatep;
	
	/*
	 We want to be able to store and reload notifications so that they can survive
	 a shutdown/restart of the client. So we can't simply pass in callbacks;
	 we have to specify a callback mechanism that can be used by name rather than 
	 by some arbitrary pointer -- and then people have to initialize callbacks 
	 in some useful location. So we use LLNotificationFunctorRegistry to manage them.
	 */
	 std::string mResponseFunctorName;
	
	/*
	 In cases where we want to specify an explict, non-persisted callback, 
	 we store that in the callback registry under a dynamically generated
	 key, and store the key in the notification, so we can still look it up
	 using the same mechanism.
	 */
	bool mTemporaryResponder;

	void init(const std::string& template_name, const LLSD& form_elements);

	LLNotification(const Params& p);

	// this is just for making it easy to look things up in a set organized by UUID -- DON'T USE IT
	// for anything real!
 LLNotification(LLUUID uuid) : mId(uuid), mCancelled(false), mRespondedTo(false), mIgnored(false), mPriority(NOTIFICATION_PRIORITY_UNSPECIFIED), mTemporaryResponder(false) {}

	void cancel();

public:

	// constructor from a saved notification
	LLNotification(const LLSD& sd);

	void setResponseFunctor(std::string const &responseFunctorName);

	typedef enum e_response_template_type
	{
		WITHOUT_DEFAULT_BUTTON,
		WITH_DEFAULT_BUTTON
	} EResponseTemplateType;

	// return response LLSD filled in with default form contents and (optionally) the default button selected
	LLSD getResponseTemplate(EResponseTemplateType type = WITHOUT_DEFAULT_BUTTON);

	// returns index of first button with value==TRUE
	// usually this the button the user clicked on
	// returns -1 if no button clicked (e.g. form has not been displayed)
	static S32 getSelectedOption(const LLSD& notification, const LLSD& response);
	// returns name of first button with value==TRUE
	static std::string getSelectedOptionName(const LLSD& notification);

	// after someone responds to a notification (usually by clicking a button,
	// but sometimes by filling out a little form and THEN clicking a button),
	// the result of the response (the name and value of the button clicked,
	// plus any other data) should be packaged up as LLSD, then passed as a
	// parameter to the notification's respond() method here. This will look up
	// and call the appropriate responder.
	//
	// response is notification serialized as LLSD:
	// ["name"] = notification name
	// ["form"] = LLSD tree that includes form description and any prefilled form data
	// ["response"] = form data filled in by user
	// (including, but not limited to which button they clicked on)
	// ["payload"] = transaction specific data, such as ["source_id"] (originator of notification),  
	//				["item_id"] (attached inventory item), etc.
	// ["substitutions"] = string substitutions used to generate notification message
	// from the template
	// ["time"] = time at which notification was generated;
	// ["expiry"] = time at which notification expires;
	// ["responseFunctor"] = name of registered functor that handles responses to notification;
	LLSD asLLSD();

	void respond(const LLSD& sd);

	void setIgnored(bool ignore);

	bool isCancelled() const
	{
		return mCancelled;
	}

	bool isRespondedTo() const
	{
		return mRespondedTo;
	}

	bool isIgnored() const
	{
		return mIgnored;
	}

	const std::string& getName() const;

	const std::string& getIcon() const;

	bool isPersistent() const;

	const LLUUID& id() const
	{
		return mId;
	}
	
	const LLSD& getPayload() const
	{
		return mPayload;
	}

	const LLSD& getSubstitutions() const
	{
		return mSubstitutions;
	}

	const LLDate& getDate() const
	{
		return mTimestamp;
	}

// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
	bool hasLabel() const;
// [/SL:KB]

	std::string getType() const;
	std::string getMessage() const;
	std::string getLabel() const;
	std::string getURL() const;
	S32 getURLOption() const;

	const LLNotificationFormPtr getForm();

	const LLDate getExpiration() const
	{
		return mExpiresAt;
	}

	ENotificationPriority getPriority() const
	{
		return mPriority;
	}

	const LLUUID getID() const
	{
		return mId;
	}
	
	// comparing two notifications normally means comparing them by UUID (so we can look them
	// up quickly this way)
	bool operator<(const LLNotification& rhs) const
	{
		return mId < rhs.mId;
	}

	bool operator==(const LLNotification& rhs) const
	{
		return mId == rhs.mId;
	}

	bool operator!=(const LLNotification& rhs) const
	{
		return !operator==(rhs);
	}

	bool isSameObjectAs(const LLNotification* rhs) const
	{
		return this == rhs;
	}
	
	// this object has been updated, so tell all our clients
	void update();

	void updateFrom(LLNotificationPtr other);
	
	// A fuzzy equals comparator.
	// true only if both notifications have the same template and 
	//     1) flagged as unique (there can be only one of these) OR 
	//     2) all required payload fields of each also exist in the other.
	bool isEquivalentTo(LLNotificationPtr that) const;
	
	// if the current time is greater than the expiration, the notification is expired
	bool isExpired() const
	{
		if (mExpiresAt.secondsSinceEpoch() == 0)
		{
			return false;
		}
		
		LLDate rightnow = LLDate::now();
		return rightnow > mExpiresAt;
	}
	
	std::string summarize() const;

	bool hasUniquenessConstraints() const;

	virtual ~LLNotification() {}
};

std::ostream& operator<<(std::ostream& s, const LLNotification& notification);

namespace LLNotificationFilters
{
	// a sample filter
	bool includeEverything(LLNotificationPtr p);

	typedef enum e_comparison 
	{ 
		EQUAL, 
		LESS, 
		GREATER, 
		LESS_EQUAL, 
		GREATER_EQUAL 
	} EComparison;

	// generic filter functor that takes method or member variable reference
	template<typename T>
	struct filterBy
	{
		typedef boost::function<T (LLNotificationPtr)>	field_t;
		typedef typename boost::remove_reference<T>::type		value_t;
		
		filterBy(field_t field, value_t value, EComparison comparison = EQUAL) 
			:	mField(field), 
				mFilterValue(value),
				mComparison(comparison)
		{
		}		
		
		bool operator()(LLNotificationPtr p)
		{
			switch(mComparison)
			{
			case EQUAL:
				return mField(p) == mFilterValue;
			case LESS:
				return mField(p) < mFilterValue;
			case GREATER:
				return mField(p) > mFilterValue;
			case LESS_EQUAL:
				return mField(p) <= mFilterValue;
			case GREATER_EQUAL:
				return mField(p) >= mFilterValue;
			default:
				return false;
			}
		}

		field_t mField;
		value_t	mFilterValue;
		EComparison mComparison;
	};
};

namespace LLNotificationComparators
{
	typedef enum e_direction { ORDER_DECREASING, ORDER_INCREASING } EDirection;

	// generic order functor that takes method or member variable reference
	template<typename T>
	struct orderBy
	{
		typedef boost::function<T (LLNotificationPtr)> field_t;
			orderBy(field_t field, EDirection direction = ORDER_INCREASING) : mField(field), mDirection(direction) {}
		bool operator()(LLNotificationPtr lhs, LLNotificationPtr rhs)
		{
			if (mDirection == ORDER_DECREASING)
			{
				return mField(lhs) > mField(rhs);
			}
			else
			{
				return mField(lhs) < mField(rhs);
			}
		}

		field_t mField;
		EDirection mDirection;
	};

	struct orderByUUID : public orderBy<const LLUUID&>
	{
		orderByUUID(EDirection direction = ORDER_INCREASING) : orderBy<const LLUUID&>(&LLNotification::id, direction) {}
	};

	struct orderByDate : public orderBy<const LLDate&>
	{
		orderByDate(EDirection direction = ORDER_INCREASING) : orderBy<const LLDate&>(&LLNotification::getDate, direction) {}
	};
};

typedef boost::function<bool (LLNotificationPtr)> LLNotificationFilter;
typedef boost::function<bool (LLNotificationPtr, LLNotificationPtr)> LLNotificationComparator;
typedef std::set<LLNotificationPtr, LLNotificationComparator> LLNotificationSet;
typedef std::multimap<std::string, LLNotificationPtr> LLNotificationMap;

// ========================================================
// Abstract base class (interface) for a channel; also used for the master container.
// This lets us arrange channels into a call hierarchy.

// We maintain a heirarchy of notification channels; events are always started at the top
// and propagated through the hierarchy only if they pass a filter.
// Any channel can be created with a parent. A null parent (empty string) means it's
// tied to the root of the tree (the LLNotifications class itself).
// The default hierarchy looks like this:
//
// LLNotifications --+-- Expiration --+-- Mute --+-- Ignore --+-- Visible --+-- History
//                                                                          +-- Alerts
//                                                                          +-- Notifications
//
// In general, new channels that want to only see notifications that pass through 
// all of the built-in tests should attach to the "Visible" channel
//
class LLNotificationChannelBase :
	public boost::signals2::trackable
{
	LOG_CLASS(LLNotificationChannelBase);
	friend struct UpdateItem;
public:
	LLNotificationChannelBase(LLNotificationFilter filter, LLNotificationComparator comp) : 
		mFilter(filter), mItems_sf(comp) 
	{}
	virtual ~LLNotificationChannelBase() {}
	// you can also connect to a Channel, so you can be notified of
	// changes to this channel
	virtual void connectChanged(const LLStandardSignal::slot_type& slot);
	virtual void connectPassedFilter(const LLStandardSignal::slot_type& slot);
	virtual void connectFailedFilter(const LLStandardSignal::slot_type& slot);

	// use this when items change or to add a new one
	bool updateItem(const LLSD& payload);
	const LLNotificationFilter& getFilter() { return mFilter; }

protected:
	AIThreadSafeSimpleDC<LLNotificationSet> mItems_sf;
	typedef AIAccess<LLNotificationSet> mItems_wat;
	typedef AIAccessConst<LLNotificationSet> mItems_crat;
	LLStandardSignal mChanged;
	LLStandardSignal mPassedFilter;
	LLStandardSignal mFailedFilter;
	
	// these are action methods that subclasses can override to take action 
	// on specific types of changes; the management of the mItems list is
	// still handled by the generic handler.
	virtual void onLoad(LLNotificationPtr p) {}
	virtual void onAdd(LLNotificationPtr p) {}
	virtual void onDelete(LLNotificationPtr p) {}
	virtual void onChange(LLNotificationPtr p) {}

	bool updateItem(const LLSD& payload, LLNotificationPtr pNotification);
	LLNotificationFilter mFilter;
};

// The type of the pointers that we're going to manage in the NotificationQueue system
// Because LLNotifications is a singleton, we don't actually expect to ever 
// destroy it, but if it becomes necessary to do so, the shared_ptr model
// will ensure that we don't leak resources.
class LLNotificationChannel;
typedef boost::shared_ptr<LLNotificationChannel> LLNotificationChannelPtr;

// manages a list of notifications
// Note that if this is ever copied around, we might find ourselves with multiple copies
// of a queue with notifications being added to different nonequivalent copies. So we 
// make it inherit from boost::noncopyable, and then create a map of shared_ptr to manage it.
// 
// NOTE: LLNotificationChannel is self-registering. The *correct* way to create one is to 
// do something like:
//		LLNotificationChannel::buildChannel("name", "parent"...);
// This returns an LLNotificationChannelPtr, which you can store, or
// you can then retrieve the channel by using the registry:
//		LLNotifications::instance().getChannel("name")...
//
class LLNotificationChannel : 
	boost::noncopyable, 
	public LLNotificationChannelBase
{
	LOG_CLASS(LLNotificationChannel);

public:  
	virtual ~LLNotificationChannel() {}
	typedef LLNotificationSet::iterator Iterator;
	
	std::string getName() const { return mName; }
	std::string getParentChannelName() { return mParent; }
	
	bool isEmpty() const;
	
	Iterator begin();
	Iterator end();

	// Channels have a comparator to control sort order;
	// the default sorts by arrival date
	void setComparator(LLNotificationComparator comparator);
	
	std::string summarize();

	// factory method for constructing these channels; since they're self-registering,
	// we want to make sure that you can't use new to make them
	static LLNotificationChannelPtr buildChannel(const std::string& name, const std::string& parent,
						LLNotificationFilter filter=LLNotificationFilters::includeEverything, 
						LLNotificationComparator comparator=LLNotificationComparators::orderByUUID());
	
protected:
	// Notification Channels have a filter, which determines which notifications
	// will be added to this channel. 
	// Channel filters cannot change.
	// Channels have a protected constructor so you can't make smart pointers that don't 
	// come from our internal reference; call NotificationChannel::build(args)
	LLNotificationChannel(const std::string& name, const std::string& parent,
						  LLNotificationFilter filter, LLNotificationComparator comparator);

private:
	std::string mName;
	std::string mParent;
	LLNotificationComparator mComparator;
};

class LLNotificationTemplates :
	public LLSingleton<LLNotificationTemplates>
{
	LOG_CLASS(LLNotificationTemplates);

	friend class LLSingleton<LLNotificationTemplates>;

	// This class may not use LLNotifications.
	typedef char LLNotifications;

public:
	bool loadTemplates();  
	LLXMLNodePtr checkForXMLTemplate(LLXMLNodePtr item);

	// This is all stuff for managing the templates
	// take your template out
	LLNotificationTemplatePtr getTemplate(const std::string& name);
	
	// get the whole collection
	typedef std::vector<std::string> TemplateNames;
	TemplateNames getTemplateNames() const;  // returns a list of notification names
	
	typedef std::map<std::string, LLNotificationTemplatePtr> TemplateMap;

	TemplateMap::const_iterator templatesBegin() { return mTemplates.begin(); }
	TemplateMap::const_iterator templatesEnd() { return mTemplates.end(); }

	// test for existence
	bool templateExists(const std::string& name);
	// useful if you're reloading the file
	void clearTemplates();   // erase all templates

	// put your template in (should only be called from LLNotifications).
	bool addTemplate(const std::string& name, LLNotificationTemplatePtr theTemplate);

	std::string getGlobalString(const std::string& key) const;

private:
	// we're a singleton, so we don't have a public constructor
	LLNotificationTemplates() { }
	/*virtual*/ void initSingleton();

	TemplateMap mTemplates;

	typedef std::map<std::string, LLXMLNodePtr> XMLTemplateMap;
	XMLTemplateMap mXmlTemplates;

	typedef std::map<std::string, std::string> GlobalStringMap;
	GlobalStringMap mGlobalStrings;
};

class LLNotifications :
	public LLSingleton<LLNotifications>,
	public LLNotificationChannelBase
{
	LOG_CLASS(LLNotifications);

	friend class LLSingleton<LLNotifications>;
public:
	// load notification descriptions from file;
	// OK to call more than once because it will reload
	bool loadNotifications();
	void createDefaultChannels();
	void onLoginCompleted();

	// we provide a collection of simple add notification functions so that it's reasonable to create notifications in one line
	LLNotificationPtr add(const std::string& name,
						const LLSD& substitutions = LLSD(),
						const LLSD& payload = LLSD());
	LLNotificationPtr add(const std::string& name,
						const LLSD& substitutions,
						const LLSD& payload,
						const std::string& functor_name);
	LLNotificationPtr add(const std::string& name,
						const LLSD& substitutions,
						const LLSD& payload,
						LLNotificationFunctorRegistry::ResponseFunctor functor);
	LLNotificationPtr add(AIAlert::Error const& error, int type, unsigned int suppress_mask);
	LLNotificationPtr add(const LLNotification::Params& p);

	void forceResponse(const LLNotification::Params& params, S32 option);

	typedef std::map<std::string, LLNotificationChannelPtr> ChannelMap;
	ChannelMap mChannels;

	void addChannel(LLNotificationChannelPtr pChan);
	LLNotificationChannelPtr getChannel(const std::string& channelName);

	void add(const LLNotificationPtr pNotif);
	void cancel(LLNotificationPtr pNotif);
	void update(const LLNotificationPtr pNotif);
	LLNotificationPtr find(LLUUID const& uuid);
	
	typedef boost::function<void (LLNotificationPtr)> NotificationProcess;
	void forEachNotification(NotificationProcess process);

private:
	// we're a singleton, so we don't have a public constructor
	LLNotifications();
	/*virtual*/ void initSingleton();
	
	void loadPersistentNotifications();

	bool expirationFilter(LLNotificationPtr pNotification);
	bool expirationHandler(const LLSD& payload);
	bool uniqueFilter(LLNotificationPtr pNotification);
	bool uniqueHandler(const LLSD& payload);
	bool failedUniquenessTest(const LLSD& payload);

	LLNotificationChannelPtr pHistoryChannel;
	LLNotificationChannelPtr pExpirationChannel;

	LLNotificationMap mUniqueNotifications;
};
#endif//LL_LLNOTIFICATIONS_H

