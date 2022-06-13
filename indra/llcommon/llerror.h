/** 
 * @file llerror.h
 * @date   December 2006
 * @brief error message system
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#ifndef LL_LLERROR_H
#define LL_LLERROR_H

#include <sstream>
#include <typeinfo>

#include "stdtypes.h"

#include "llpreprocessor.h"
#include <boost/static_assert.hpp>

const int LL_ERR_NOERR = 0;

// Define one of these for different error levels in release...
// #define RELEASE_SHOW_DEBUG // Define this if you want your release builds to show lldebug output.
#define RELEASE_SHOW_INFO // Define this if you want your release builds to show llinfo output
#define RELEASE_SHOW_WARN // Define this if you want your release builds to show llwarn output.

#ifdef _DEBUG
#define SHOW_DEBUG
#define SHOW_WARN
#define SHOW_INFO
#define SHOW_ASSERT
#else // _DEBUG

#ifdef LL_RELEASE_WITH_DEBUG_INFO
#define SHOW_ASSERT
#endif // LL_RELEASE_WITH_DEBUG_INFO

#ifdef RELEASE_SHOW_DEBUG
#define SHOW_DEBUG
#endif

#ifdef RELEASE_SHOW_WARN
#define SHOW_WARN
#endif

#ifdef RELEASE_SHOW_INFO
#define SHOW_INFO
#endif

#ifdef RELEASE_SHOW_ASSERT
#define SHOW_ASSERT
#endif

#endif // !_DEBUG

inline const std::string liru_assert_strip(const std::string& file) { return file.substr(1+file.substr(0, file.find_last_of("/\\")).find_last_of("/\\")); } //return foo/bar.cpp or perhaps foo\bar.cpp

#define llassert_always_msg(func, msg) if (LL_UNLIKELY(!(func))) LL_ERRS() << "ASSERT (" << msg << ")\nfile:" << liru_assert_strip(__FILE__) << " line:" << std::dec << __LINE__ << LL_ENDL

#define llassert_always(func)	llassert_always_msg(func, #func)

#ifdef SHOW_ASSERT
#define llassert(func)			llassert_always_msg(func, #func)
#define llverify(func)			llassert_always_msg(func, #func)
#define ASSERT_ONLY(...)		__VA_ARGS__
#define ASSERT_ONLY_COMMA(...)	, __VA_ARGS__
#else
#define llassert(func)
#define llverify(func)			do {if (func) {}} while(0)
#define ASSERT_ONLY(...)
#define ASSERT_ONLY_COMMA(...)
#endif

#ifdef LL_WINDOWS
#define LL_STATIC_ASSERT(func, msg) static_assert(func, msg)
#define LL_BAD_TEMPLATE_INSTANTIATION(type, msg) static_assert(false, msg)
#else
#define LL_STATIC_ASSERT(func, msg) BOOST_STATIC_ASSERT(func)
#define LL_BAD_TEMPLATE_INSTANTIATION(type, msg) BOOST_STATIC_ASSERT(sizeof(type) != 0 && false);
#endif

/** Error Logging Facility

	Information for most users:
	
	Code can log messages with constructions like this:
	
		LL_INFOS("StringTag") << "request to fizzbip agent " << agent_id
			<< " denied due to timeout" << LL_ENDL;
		
	Messages can be logged to one of four increasing levels of concern,
	using one of four "streams":

		LL_DEBUGS("StringTag")	- debug messages that are normally suppressed
		LL_INFOS("StringTag")	- informational messages that are normal shown
		LL_WARNS("StringTag")	- warning messages that signal a problem
		LL_ERRS("StringTag")	- error messages that are major, unrecoverable failures
		
	The later (LL_ERRS("StringTag")) automatically crashes the process after the message
	is logged.
	
	Note that these "streams" are actually #define magic.  Rules for use:
		* they cannot be used as normal streams, only to start a message
		* messages written to them MUST be terminated with LL_ENDL
		* between the opening and closing, the << operator is indeed
		  writing onto a std::ostream, so all conversions and stream
		  formating are available
	
	These messages are automatically logged with function name, and (if enabled)
	file and line of the message.  (Note: Existing messages that already include
	the function name don't get name printed twice.)
	
	If you have a class, adding LOG_CLASS line to the declaration will cause
	all messages emitted from member functions (normal and static) to be tagged
	with the proper class name as well as the function name:
	
		class LLFoo
		{
			LOG_CLASS(LLFoo);
		public:
			...
		};
	
		void LLFoo::doSomething(int i)
		{
			if (i > 100)
			{
				LL_WARNS("FooBarTag") << "called with a big value for i: " << i << LL_ENDL; 
			}
			...
		}
	
	will result in messages like:
	
		WARN: LLFoo::doSomething: called with a big value for i: 283
	
	Which messages are logged and which are suppressed can be controlled at run
	time from the live file logcontrol.xml based on function, class and/or 
	source file.  See etc/logcontrol-dev.xml for details.
	
	Lastly, logging is now very efficient in both compiled code and execution
	when skipped.  There is no need to wrap messages, even debugging ones, in
	#ifdef _DEBUG constructs.  LL_DEBUGS("StringTag") messages are compiled into all builds,
	even release.  Which means you can use them to help debug even when deployed
	to a real grid.
*/
namespace LLError
{
	enum ELevel
	{
		LEVEL_ALL = 0,
			// used to indicate that all messages should be logged
			
		LEVEL_DEBUG = 0,
		LEVEL_INFO = 1,
		LEVEL_WARN = 2,
		LEVEL_ERROR = 3,	// used to be called FATAL
		
		LEVEL_NONE = 4
			// not really a level
			// used to indicate that no messages should be logged
	};
	
	/*	Macro support
		The classes CallSite and Log are used by the logging macros below.
		They are not intended for general use.
	*/
	
	struct CallSite;
	
	class LL_COMMON_API Log
	{
	public:
		static bool shouldLog(CallSite&);
		static std::ostringstream* out();
		static void flush(std::ostringstream* out, char* message);
		static void flush(std::ostringstream*, const CallSite&);
	};
	
	struct LL_COMMON_API CallSite
	{
		// Represents a specific place in the code where a message is logged
		// This is public because it is used by the macros below.  It is not
		// intended for public use.
		CallSite(ELevel level, 
				const char* file, 
				int line,
				const std::type_info& class_info, 
				const char* function, 
				bool print_once, 
				const char** tags, 
				size_t tag_count);

		~CallSite();

#ifdef LL_LIBRARY_INCLUDE
		bool shouldLog();
#else // LL_LIBRARY_INCLUDE
		bool shouldLog()
		{ 
			return mCached 
					? mShouldLog 
					: Log::shouldLog(*this); 
		}
			// this member function needs to be in-line for efficiency
#endif // LL_LIBRARY_INCLUDE
		
		void invalidate();
		
		// these describe the call site and never change
		const ELevel			mLevel;
		const char* const		mFile;
		const int				mLine;
		const std::type_info&   mClassInfo;
		const char* const		mFunction;
		const char**			mTags;
		size_t					mTagCount;
		const bool				mPrintOnce;
		const char*				mLevelString;
		std::string				mLocationString,
								mFunctionString,
								mTagString;
		bool					mCached,
								mShouldLog;
		
		friend class Log;
	};
	
	
	class End { };
	inline std::ostream& operator<<(std::ostream& s, const End&)
		{ return s; }
		// used to indicate the end of a message
		
	class LL_COMMON_API NoClassInfo { };
		// used to indicate no class info known for logging

   //LLCallStacks keeps track of call stacks and output the call stacks to log file
   //when LLAppViewer::handleViewerCrash() is triggered.
   //
   //Note: to be simple, efficient and necessary to keep track of correct call stacks, 
	//LLCallStacks is designed not to be thread-safe.
   //so try not to use it in multiple parallel threads at same time.
   //Used in a single thread at a time is fine.
   class LL_COMMON_API LLCallStacks
   {
   private:
       static char**  sBuffer ;
	   static S32     sIndex ;

	   static void allocateStackBuffer();
	   static void freeStackBuffer();
          
   public:   
	   static void push(const char* function, const int line) ;
	   static std::ostringstream* insert(const char* function, const int line) ;
       static void print() ;
       static void clear() ;
	   static void end(std::ostringstream* _out) ;
	   static void cleanup();
   }; 
}

//this is cheaper than llcallstacks if no need to output other variables to call stacks. 
#define LL_PUSH_CALLSTACKS() LLError::LLCallStacks::push(__FUNCTION__, __LINE__)

#define llcallstacks                                                                      \
	{                                                                                     \
       std::ostringstream* _out = LLError::LLCallStacks::insert(__FUNCTION__, __LINE__) ; \
       (*_out)

#define llcallstacksendl                   \
		LLError::End();                    \
		LLError::LLCallStacks::end(_out) ; \
	}

#define LL_CLEAR_CALLSTACKS() LLError::LLCallStacks::clear()
#define LL_PRINT_CALLSTACKS() LLError::LLCallStacks::print() 

/*
	Class type information for logging
 */

#define LOG_CLASS(s)	typedef s _LL_CLASS_TO_LOG
	// Declares class to tag logged messages with.
	// See top of file for example of how to use this
	
typedef LLError::NoClassInfo _LL_CLASS_TO_LOG;
	// Outside a class declaration, or in class without LOG_CLASS(), this
	// typedef causes the messages to not be associated with any class.

/////////////////////////////////
// Error Logging Macros
// See top of file for common usage.	
/////////////////////////////////

// this macro uses a one-shot do statement to avoid parsing errors when writing control flow statements
// without braces:
// if (condition) LL_INFOS() << "True" << LL_ENDL; else LL_INFOS()() << "False" << LL_ENDL

#define lllog(level, once, nofunction, ...)																	          \
	do {                                                                                                  \
		const char* tags[] = {"", ##__VA_ARGS__};													      \
		::size_t tag_count = LL_ARRAY_SIZE(tags) - 1;													  \
		static LLError::CallSite _site(                                                                   \
		    level, __FILE__, __LINE__, typeid(_LL_CLASS_TO_LOG), nofunction ? NULL : __FUNCTION__, once, &tags[1], tag_count);\
		if (LL_UNLIKELY(_site.shouldLog()))			                                                      \
		{                                                                                                 \
			std::ostringstream* _out = LLError::Log::out();                                               \
			(*_out)

//Use this construct if you need to do computation in the middle of a
//message:
//	
//	LL_INFOS("AgentGesture") << "the agent " << agend_id;
//	switch (f)
//	{
//		case FOP_SHRUGS:	LL_CONT << "shrugs";				break;
//		case FOP_TAPS:		LL_CONT << "points at " << who;	break;
//		case FOP_SAYS:		LL_CONT << "says " << message;	break;
//	}
//	LL_CONT << " for " << t << " seconds" << LL_ENDL;
//	
//Such computation is done iff the message will be logged.
#define LL_CONT	(*_out)

#define LL_NEWLINE '\n'

#define LL_ENDL                               \
			LLError::End();                   \
			LLError::Log::flush(_out, _site); \
		}                                     \
	} while(0)

// NEW Macros for debugging, allow the passing of a string tag

#ifdef SHOW_DEBUG 
#define DO_DEBUG_LOG
#else
#define DO_DEBUG_LOG if (false)
#endif 

// Pass comma separated list of tags (currently only supports up to 0, 1, or 2)
#define LL_DEBUGS(...)	DO_DEBUG_LOG lllog(LLError::LEVEL_DEBUG, false, false, ##__VA_ARGS__)
#define LL_INFOS(...)	lllog(LLError::LEVEL_INFO, false, false, ##__VA_ARGS__)
#define LL_WARNS(...)	lllog(LLError::LEVEL_WARN, false, false, ##__VA_ARGS__)
#define LL_ERRS(...)	lllog(LLError::LEVEL_ERROR, false, false, ##__VA_ARGS__)
// alternative to llassert_always that prints explanatory message
#define LL_ERRS_IF(exp, ...)	if (exp) LL_ERRS(##__VA_ARGS__) << "(" #exp ")"

// Only print the log message once (good for warnings or infos that would otherwise
// spam the log file over and over, such as tighter loops).
#define LL_DEBUGS_ONCE(...)	DO_DEBUG_LOG lllog(LLError::LEVEL_DEBUG, true, false, ##__VA_ARGS__)
#define LL_INFOS_ONCE(...)	lllog(LLError::LEVEL_INFO, true, false, ##__VA_ARGS__)
#define LL_WARNS_ONCE(...)	lllog(LLError::LEVEL_WARN, true, false, ##__VA_ARGS__)

// No function name
#define LL_DEBUGS_NF(...)	DO_DEBUG_LOG {lllog(LLError::LEVEL_DEBUG, false, true, ##__VA_ARGS__)
#define LL_INFOS_NF(...)	lllog(LLError::LEVEL_INFO, false, true, ##__VA_ARGS__)
#define LL_WARNS_NF(...)	lllog(LLError::LEVEL_WARN, false, true, ##__VA_ARGS__)
#define LL_ERRS_NF(...)	lllog(LLError::LEVEL_ERROR, false, true, ##__VA_ARGS__)

// DEPRECATED: Use the new macros that allow tags and *look* like macros.
#define lldebugs	LL_COMPILE_TIME_MESSAGE("Warning: lldebugs deprecated, use LL_DEBUGS() instead") LL_DEBUGS()
#define llinfos		LL_COMPILE_TIME_MESSAGE("Warning: llinfos deprecated, use LL_INFOS() instead") LL_INFOS()
#define llwarns		LL_COMPILE_TIME_MESSAGE("Warning: llwarns deprecated, use LL_WARNS() instead") LL_WARNS()
#define llerrs		LL_COMPILE_TIME_MESSAGE("Warning: llerrs deprecated, use LL_ERRS() instead") LL_ERRS()
#define llcont		LL_COMPILE_TIME_MESSAGE("Warning: llcont deprecated, use LL_CONT instead") LL_CONT
#define llendl		LL_COMPILE_TIME_MESSAGE("Warning: llendl deprecated, use LL_ENDL instead") LL_ENDL


#endif // LL_LLERROR_H
