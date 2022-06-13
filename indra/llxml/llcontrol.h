/** 
 * @file llcontrol.h
 * @brief A mechanism for storing "control state" for a program
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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
 */

#ifndef LL_LLCONTROL_H
#define LL_LLCONTROL_H

#include "llboost.h"
#include "llevent.h"
#include "llnametable.h"
#include "llmap.h"
#include "llstring.h"
#include "llrect.h"
#include "v4color.h"
#include "v4coloru.h"
#include "llinstancetracker.h"
#include "llrefcount.h"

#include "llcontrolgroupreader.h"

#include <vector>

// *NOTE: boost::visit_each<> generates warning 4675 on .net 2003
// Disable the warning for the boost includes.
#if LL_WINDOWS
# if (_MSC_VER >= 1300 && _MSC_VER < 1400)
#   pragma warning(push)
#   pragma warning( disable : 4675 )
# endif
#endif

#include <boost/bind.hpp>

#if LL_WINDOWS
	#pragma warning (push)
	#pragma warning (disable : 4263) // boost::signals2::expired_slot::what() has const mismatch
	#pragma warning (disable : 4264) 
#endif
#include <boost/signals2.hpp>
#if LL_WINDOWS
	#pragma warning (pop)
#endif

#if LL_WINDOWS
# if (_MSC_VER >= 1300 && _MSC_VER < 1400)
#   pragma warning(pop)
# endif
#endif

#if LL_RELEASE_WITH_DEBUG_INFO
#define PROF_CTRL_CALLS
#endif //LL_RELEASE_WITH_DEBUG_INFO

class LLVector3;
class LLVector3d;
class LLColor3;

const BOOL NO_PERSIST = FALSE;

// Saved at end of session
class LLControlGroup; //Defined further down
extern LLControlGroup gSavedSettings;		//Default control group used in LLCachedControl
extern LLControlGroup gSavedPerAccountSettings;	//For ease

typedef enum e_control_type
{
	TYPE_U32 = 0,
	TYPE_S32,
	TYPE_F32,
	TYPE_BOOLEAN,
	TYPE_STRING,
	TYPE_VEC3,
	TYPE_VEC3D,
	TYPE_RECT,
	TYPE_COL4,
	TYPE_COL4U, //exist merely for legacy colors.xml support. Not a real control type.
	TYPE_COL3,
	TYPE_LLSD,
	TYPE_COUNT
} eControlType;

class LLControlVariable : public LLRefCount
{
	LOG_CLASS(LLControlVariable);

	friend class LLControlGroup;
	
public:
	typedef boost::signals2::signal<bool(LLControlVariable* control, const LLSD&), boost_boolean_combiner> validate_signal_t;
	typedef boost::signals2::signal<void(LLControlVariable* control, const LLSD&)> commit_signal_t;

private:
	std::string		mName;
	std::string		mComment;
	eControlType	mType;
	bool			mPersist;
	bool			mHideFromSettingsEditor;
	std::vector<LLSD> mValues;
	
	boost::shared_ptr<commit_signal_t> mCommitSignal;	//Signals are non-copyable. Therefore, using a pointer so vars can 'share' signals for COA
	boost::shared_ptr<validate_signal_t> mValidateSignal;

	//COA stuff:
	bool			mIsCOA;				//To have COA connection set.
	bool			mIsCOAParent;		//if true, use if settingsperaccount is false.
	LLControlVariable *mCOAConnectedVar;//Because the two vars refer to eachother, LLPointer would be a circular refrence..
public:
	LLControlVariable(const std::string& name, eControlType type,
					  LLSD initial, const std::string& comment,
					  bool persist = true, bool hidefromsettingseditor = false, bool IsCOA = false);

	virtual ~LLControlVariable();
	
	const std::string& getName() const { return mName; }
	const std::string& getComment() const { return mComment; }

	eControlType type() const		{ return mType; }
	bool isType(eControlType tp) const { return tp == mType; }

	void resetToDefault(bool fire_signal = false);

	commit_signal_t* getSignal() { return mCommitSignal.get(); } // shorthand for commit signal
	commit_signal_t* getCommitSignal() { return mCommitSignal.get(); }
	validate_signal_t* getValidateSignal() { return mValidateSignal.get(); }

	bool isDefault() { return (mValues.size() == 1); }
	bool isSaveValueDefault();
	bool isPersisted() { return mPersist; }
	bool isHiddenFromSettingsEditor() { return mHideFromSettingsEditor; }
	LLSD get()			const	{ return getValue(); }
	LLSD getValue()		const	{ return mValues.back(); }
	LLSD getDefault()	const	{ return mValues.front(); }
	LLSD getSaveValue() const;

	void set(const LLSD& val)	{ setValue(val); }
	void setValue(const LLSD& value, bool saved_value = TRUE);
	void setDefaultValue(const LLSD& value);
	void setPersist(bool state);
	void setHiddenFromSettingsEditor(bool hide);
	void setComment(const std::string& comment);

	void firePropertyChanged()
	{
		(*mCommitSignal)(this,mValues.back());
	}

	//COA stuff
	bool isCOA()		const	{ return mIsCOA; }
	bool isCOAParent()	const	{ return mIsCOAParent; }
	LLControlVariable *getCOAConnection() const	{ return mCOAConnectedVar; }
	LLControlVariable* getCOAActive();
	LLControlVariable const* getCOAActive() const;
	void setIsCOA(bool IsCOA)  { mIsCOA=IsCOA; }
	void setCOAConnect(LLControlVariable *pConnect, bool IsParent) 
	{
		mIsCOAParent=IsParent;
		mCOAConnectedVar=pConnect;
		if(!IsParent)
		{
			mCommitSignal = pConnect->mCommitSignal; //Share a single signal.
			mValidateSignal = pConnect->mValidateSignal;
		}
	}
#ifdef PROF_CTRL_CALLS
public:
	U32 mLookupCount;
#endif //PROF_CTRL_CALLS
private:
	LLSD getComparableValue(const LLSD& value);
	bool llsd_compare(const LLSD& a, const LLSD & b);
};

typedef LLPointer<LLControlVariable> LLControlVariablePtr;

//! Helper functions for converting between static types and LLControl values
template <class T> 
eControlType get_control_type()
{
	LL_WARNS() << "Usupported control type: " << typeid(T).name() << "." << LL_ENDL;
	return TYPE_COUNT;
}

template <class T> 
LLSD convert_to_llsd(const T& in)
{
	// default implementation
	return LLSD(in);
}

template <class T>
T convert_from_llsd(const LLSD& sd, eControlType type, const std::string& control_name)
{
	// needs specialization
	return T(sd);
}
//const U32 STRING_CACHE_SIZE = 10000;
class LLControlGroup : public LLInstanceTracker<LLControlGroup, std::string>
{
	LOG_CLASS(LLControlGroup);
protected:
	typedef std::map<std::string, LLControlVariablePtr > ctrl_name_table_t;
	ctrl_name_table_t mNameTable;
	std::set<std::string> mWarnings;
	std::string mTypeString[TYPE_COUNT];

	eControlType typeStringToEnum(const std::string& typestr);
	std::string typeEnumToString(eControlType typeenum);
	std::set<std::string> mIncludedFiles; //To prevent perpetual recursion.
public:
	LLControlGroup(const std::string& name);
	~LLControlGroup();
	void cleanup();
	
	typedef LLInstanceTracker<LLControlGroup, std::string>::instance_iter instance_iter;
	LLControlVariable* getControl(std::string const& name);
	LLControlVariable const* getControl(std::string const& name) const;

	struct ApplyFunctor
	{
		virtual ~ApplyFunctor() {};
		virtual void apply(const std::string& name, LLControlVariable* control) = 0;
	};
	void applyToAll(ApplyFunctor* func);

	BOOL declareControl(const std::string& name, eControlType type, const LLSD initial_val, const std::string& comment, BOOL persist, BOOL hidefromsettingseditor = FALSE, bool IsCOA = false);
	BOOL declareU32(const std::string& name, U32 initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareS32(const std::string& name, S32 initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareF32(const std::string& name, F32 initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareBOOL(const std::string& name, BOOL initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareString(const std::string& name, const std::string &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareVec3(const std::string& name, const LLVector3 &initial_val,const std::string& comment,  BOOL persist = TRUE);
	BOOL declareVec3d(const std::string& name, const LLVector3d &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareRect(const std::string& name, const LLRect &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareColor4(const std::string& name, const LLColor4 &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareColor3(const std::string& name, const LLColor3 &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareLLSD(const std::string& name, const LLSD &initial_val, const std::string& comment, BOOL persist = TRUE);
	
	std::string 	findString(const std::string& name);

	std::string getString(const std::string& name) const;
	std::string getText(const std::string& name);
	BOOL		getBOOL(const std::string& name);
	S32			getS32(const std::string& name);
	F32			getF32(const std::string& name);
	U32			getU32(const std::string& name);
	
	LLWString	getWString(const std::string& name);
	LLVector3	getVector3(const std::string& name);
	LLVector3d	getVector3d(const std::string& name);
	LLRect		getRect(const std::string& name);
	LLSD        getLLSD(const std::string& name);


	// Note: If an LLColor4U control exists, it will cast it to the correct
	// LLColor4 for you.
	LLColor4	getColor(const std::string& name);
	LLColor4	getColor4(const std::string& name);
	LLColor3	getColor3(const std::string& name);

	// generic getter
	template<typename T> T get(const std::string& name) const
	{
		const LLControlVariable* control = getControl(name);
		LLSD value;
		eControlType type = TYPE_COUNT;

		if (control)		
		{
			value = control->get();
			type = control->type();
		}
		else
		{
			LL_WARNS() << "Control " << name << " not found." << LL_ENDL;
			return T();
		}
		return convert_from_llsd<T>(value, type, name);
	}

	void	setBOOL(const std::string& name, BOOL val);
	void	setS32(const std::string& name, S32 val);
	void	setF32(const std::string& name, F32 val);
	void	setU32(const std::string& name, U32 val);
	void	setString(const std::string&  name, const std::string& val);
	void	setVector3(const std::string& name, const LLVector3 &val);
	void	setVector3d(const std::string& name, const LLVector3d &val);
	void	setRect(const std::string& name, const LLRect &val);
	void	setColor4(const std::string& name, const LLColor4 &val);
	void	setColor3(const std::string& name, const LLColor3 &val);
	void    setLLSD(const std::string& name, const LLSD& val);

	// type agnostic setter that takes LLSD
	void	setUntypedValue(const std::string& name, const LLSD& val);

	// generic setter
	template<typename T> void set(const std::string& name, const T& val)
	{
		LLControlVariable* control = getControl(name);
	
		if (control && control->isType(get_control_type<T>()))
		{
			control->set(convert_to_llsd(val));
		}
		else
		{
			LL_WARNS() << "Invalid control " << name << LL_ENDL;
		}
	}
	
	BOOL    controlExists(const std::string& name) const;

	// Returns number of controls loaded, 0 if failed
	// If require_declaration is false, will auto-declare controls it finds
	// as the given type.
	U32	loadFromFileLegacy(const std::string& filename, BOOL require_declaration = TRUE, eControlType declare_as = TYPE_STRING);
 	U32 saveToFile(const std::string& filename, BOOL nondefault_only);
 	U32	loadFromFile(const std::string& filename, bool default_values = false, bool save_values = true);
	void	resetToDefaults();

	
	// Ignorable Warnings
	
	// Add a config variable to be reset on resetWarnings()
	LLControlVariable *addWarning(const std::string& name);
	BOOL getWarning(const std::string& name);
	void setWarning(const std::string& name, BOOL val);
	
	// Resets all ignorables
	void resetWarnings();

	//COA stuff
	void connectToCOA(LLControlVariable *pConnecter, const std::string& name, eControlType type, const LLSD initial_val, const std::string& comment, BOOL persist);
	void connectCOAVars(LLControlGroup &OtherGroup);
	void updateCOASetting(bool coa_enabled);
	bool handleCOASettingChange(const LLSD& newvalue);

#ifdef PROF_CTRL_CALLS
	void updateLookupMap(ctrl_name_table_t::const_iterator iter) const;
#endif //PROF_CTRL_CALLS
};


//! Publish/Subscribe object to interact with LLControlGroups.

//! Use an LLCachedControl instance to connect to a LLControlVariable
//! without have to manually create and bind a listener to a local
//! object.
template <class T>
class LLControlCache : public LLRefCount, public LLInstanceTracker<LLControlCache<T>, std::string>
{
public:
	// This constructor will declare a control if it doesn't exist in the contol group
	LLControlCache(LLControlGroup& group,
					const std::string& name, 
					const T& default_value, 
					const std::string& comment)
	:	LLInstanceTracker<LLControlCache<T>, std::string >(name)
	{
		if(!group.controlExists(name))
		{
			//LL_ERRS() << "Control named " << name << " not found." << LL_ENDL;
			if(!declareTypedControl(group, name, default_value, comment))
			{
				LL_ERRS() << "The control could not be created!!!" << LL_ENDL;
			}
		}

		bindToControl(group, name);
	}

	LLControlCache(LLControlGroup& group,
					const std::string& name)
	:	LLInstanceTracker<LLControlCache<T>, std::string >(name)
	{
		if(!group.controlExists(name))
		{
			LL_ERRS() << "Control named " << name << " not found." << LL_ENDL;
		}

		bindToControl(group, name);
	}

	~LLControlCache()
	{
	}

	const T& getValue() const { return mCachedValue; }
	void setTypeValue(const T& v) { mControl->set(convert_to_llsd(v)); }
	
private:
	void bindToControl(LLControlGroup& group, const std::string& name)
	{
		LLControlVariablePtr controlp = group.getControl(name);
		mType = controlp->type();
		mCachedValue = convert_from_llsd<T>(controlp->get(), mType, name);
		
		// Add a listener to the controls signal...
		// and store the connection...
		mConnection = controlp->getSignal()->connect(
			boost::bind(&LLControlCache<T>::handleValueChange, this, _2),
			boost::signals2::at_front //make sure cachedctrl slots are inserted before anything else.
			);
		mType = controlp->type();
		mControl = controlp;
	}
	
	bool declareTypedControl(LLControlGroup& group,
							const std::string& name, 
							 const T& default_value,
							 const std::string& comment)
	{
		LLSD init_value;
		eControlType type = get_control_type<T>();
		init_value = convert_to_llsd(default_value);
		if(type < TYPE_COUNT)
		{
			group.declareControl(name, type, init_value, comment, FALSE);
			return true;
		}
		return false;
	}

	bool handleValueChange(const LLSD& newvalue)
	{
		mCachedValue = convert_from_llsd<T>(newvalue, mType, "");
		return true;
	}

private:
    T mCachedValue;
    LLPointer<LLControlVariable> mControl;	//Hold a pointer to the control so setters are possible in cachedctrl
	eControlType				mType;
    boost::signals2::scoped_connection mConnection;
};

template <typename T>
class LLCachedControl
{
public:
	LLCachedControl(const std::string& name, 
					const T& default_value, 
					const std::string& comment = "Declared In Code")
	{
		mCachedControlPtr = LLControlCache<T>::getInstance(name);
		if (mCachedControlPtr.isNull())
		{
			mCachedControlPtr = new LLControlCache<T>(gSavedSettings, name, default_value, comment);
		}
	}

	LLCachedControl(const std::string& name)
	{
		mCachedControlPtr = LLControlCache<T>::getInstance(name);
		if (mCachedControlPtr.isNull())
		{
			mCachedControlPtr = new LLControlCache<T>(gSavedSettings, name);
		}
	}
	
	LLCachedControl(LLControlGroup& group,
					const std::string& name, 
					const T& default_value, 
					const std::string& comment = "Declared In Code")
	{
		mCachedControlPtr = LLControlCache<T>::getInstance(name);
		if (mCachedControlPtr.isNull())
		{
			mCachedControlPtr = new LLControlCache<T>(group, name, default_value, comment);
		}
	}

	LLCachedControl(LLControlGroup& group,
					const std::string& name)
	{
		mCachedControlPtr = LLControlCache<T>::getInstance(name);
		if (mCachedControlPtr.isNull())
		{
			mCachedControlPtr = new LLControlCache<T>(group, name);
		}
	}

	operator const T&() const { return mCachedControlPtr->getValue(); }
	operator boost::function<const T&()> () const { return boost::function<const T&()>(*this); }
	const T& operator()() const { return mCachedControlPtr->getValue(); }
	/*	Sometimes implicit casting doesn't work.
		For instance, something like "LLCachedControl<LLColor4> color("blah",LLColor4()); color.getValue();" 
		will not compile as it will look for the function getValue() in LLCachedControl, which doesn't exist.
			Use 'color.get().getValue()' instead if something like this happens.
		
		Manually casting to (const T) would work too, but it's ugly and requires knowledge of LLCachedControl's internals
	*/
	const T &get() const { return mCachedControlPtr->getValue(); }
	
	LLCachedControl& operator =(const T& newvalue)
	{
		mCachedControlPtr->setTypeValue(newvalue);
		return *this;
	}

private:
	LLPointer<LLControlCache<T> > mCachedControlPtr;
};

template <> eControlType get_control_type<U32>();
template <> eControlType get_control_type<S32>();
template <> eControlType get_control_type<F32>();
template <> eControlType get_control_type<bool>(); 
// Yay BOOL, its really an S32.
//template <> eControlType get_control_type<BOOL> () 
template <> eControlType get_control_type<std::string>();
template <> eControlType get_control_type<LLVector3>();
template <> eControlType get_control_type<LLVector3d>(); 
template <> eControlType get_control_type<LLRect>();
template <> eControlType get_control_type<LLColor4>();
template <> eControlType get_control_type<LLColor3>();
template <> eControlType get_control_type<LLSD>();

template <> LLSD convert_to_llsd<U32>(const U32& in);
template <> LLSD convert_to_llsd<LLVector3>(const LLVector3& in);
template <> LLSD convert_to_llsd<LLVector3d>(const LLVector3d& in); 
template <> LLSD convert_to_llsd<LLRect>(const LLRect& in);
template <> LLSD convert_to_llsd<LLColor4>(const LLColor4& in);
template <> LLSD convert_to_llsd<LLColor3>(const LLColor3& in);

template<> std::string convert_from_llsd<std::string>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> LLWString convert_from_llsd<LLWString>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> LLVector3 convert_from_llsd<LLVector3>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> LLVector3d convert_from_llsd<LLVector3d>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> LLRect convert_from_llsd<LLRect>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> bool convert_from_llsd<bool>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> S32 convert_from_llsd<S32>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> F32 convert_from_llsd<F32>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> U32 convert_from_llsd<U32>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> LLColor3 convert_from_llsd<LLColor3>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> LLColor4 convert_from_llsd<LLColor4>(const LLSD& sd, eControlType type, const std::string& control_name);
template<> LLSD convert_from_llsd<LLSD>(const LLSD& sd, eControlType type, const std::string& control_name);

//#define TEST_CACHED_CONTROL 1
#ifdef TEST_CACHED_CONTROL
void test_cached_control();
#endif // TEST_CACHED_CONTROL
#endif
