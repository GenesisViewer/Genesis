/**
 * @file llcommandlineparser.cpp
 * @brief The LLCommandLineParser class definitions
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"
#include "llcommandlineparser.h"

// *NOTE: The boost::lexical_cast generates 
// the warning C4701(local used with out assignment) in VC7.1.
// Disable the warning for the boost includes.
#if _MSC_VER
#   pragma warning(push)
#   pragma warning( disable : 4701 )
#else
// NOTE: For the other platforms?
#endif

#include <boost/program_options.hpp>
#include <boost/bind.hpp>
#include<boost/tokenizer.hpp>

#if _MSC_VER
#   pragma warning(pop)
#endif

#include "llsdserialize.h"
#include <iostream>
#include <sstream>

#include "llcontrol.h"

namespace po = boost::program_options;

// *NOTE:MEP - Currently the boost object reside in file scope. 
// This has a couple of negatives, they are always around and 
// there can be only one instance of each. 
// The plus is that the boost-ly-ness of this implementation is 
// hidden from the rest of the world. 
// Its importatnt to realize that multiple LLCommandLineParser objects 
// will all have this single repository of option escs and parsed options.
// This could be good or bad, and probably won't matter for most use cases.
namespace 
{
    po::options_description gOptionsDesc;
    po::positional_options_description gPositionalOptions;
	po::variables_map gVariableMap;
    
    const LLCommandLineParser::token_vector_t gEmptyValue;

    void read_file_into_string(std::string& str, const std::basic_istream < char >& file)
    {
	    std::ostringstream oss;
	    oss << file.rdbuf();
	    str = oss.str();
    }

    bool gPastLastOption = false;
}

class LLCLPError : public std::logic_error {
public:
    LLCLPError(const std::string& what) : std::logic_error(what) {}
};

class LLCLPLastOption : public std::logic_error {
public:
    LLCLPLastOption(const std::string& what) : std::logic_error(what) {}
};

class LLCLPValue : public po::value_semantic_codecvt_helper<char> 
{
    unsigned mMinTokens;
    unsigned mMaxTokens;
    bool mIsComposing;
	bool mIsRequired;
    typedef boost::function1<void, const LLCommandLineParser::token_vector_t&> notify_callback_t;
    notify_callback_t mNotifyCallback;
    bool mLastOption;

public:
    LLCLPValue() :
        mMinTokens(0),
        mMaxTokens(0),
		mIsRequired(false),
        mIsComposing(false),
        mLastOption(false)
        {}
      
    virtual ~LLCLPValue() {};

    void setMinTokens(unsigned c) 
    {
        mMinTokens = c;
    }

    void setMaxTokens(unsigned c) 
    {
        mMaxTokens = c;
    }

    void setComposing(bool c)
    {
        mIsComposing = c;
    }

    void setLastOption(bool c)
    {
        mLastOption = c;
    }

    void setNotifyCallback(notify_callback_t f)
    {
        mNotifyCallback = f;
    }

	void setRequired(bool c)
	{
		mIsRequired = c;
	}
    // Overrides to support the value_semantic interface.
    virtual std::string name() const 
    { 
        const std::string arg("arg");
        const std::string args("args");
        return (max_tokens() > 1) ? args : arg; 
    }

    virtual unsigned min_tokens() const
    {
        return mMinTokens;
    }

    virtual unsigned max_tokens() const 
    {
        return mMaxTokens;
    }

    virtual bool is_composing() const 
    {
        return mIsComposing;
    }

    virtual bool apply_default(boost::any& value_store) const
    {
        return false; // No defaults.
    }

    virtual void notify(const boost::any& value_store) const
    {
        const LLCommandLineParser::token_vector_t* value =
            boost::any_cast<const LLCommandLineParser::token_vector_t>(&value_store);
        if(mNotifyCallback) 
        {
           mNotifyCallback(*value);
        }
    }

    virtual bool is_required(void) const
    {
        return mIsRequired;
    }

    virtual bool adjacent_tokens_only() const
    {
        return false;
    }

protected:
    void xparse(boost::any& value_store,
         const std::vector<std::string>& new_tokens) const
    {
        if(gPastLastOption)
        {
            throw(LLCLPLastOption("Don't parse no more!"));
        }

        // Error checks. Needed?
        if (!value_store.empty() && !is_composing()) 
        {
            throw(LLCLPError("Non composing value with multiple occurences."));
        }
        if (new_tokens.size() < min_tokens() || new_tokens.size() > max_tokens())
        {
            throw(LLCLPError("Illegal number of tokens specified."));
        }
        
        if(value_store.empty())
        {
            value_store = boost::any(LLCommandLineParser::token_vector_t());
        }
        LLCommandLineParser::token_vector_t* tv = 
            boost::any_cast<LLCommandLineParser::token_vector_t>(&value_store); 
       
        for(unsigned i = 0; i < new_tokens.size() && i < mMaxTokens; ++i)
        {
            tv->push_back(new_tokens[i]);
        }

        if(mLastOption)
        {
            gPastLastOption = true;
        }
    }
};

//----------------------------------------------------------------------------
// LLCommandLineParser defintions
//----------------------------------------------------------------------------
void LLCommandLineParser::addOptionDesc(const std::string& option_name, 
                                        boost::function1<void, const token_vector_t&> notify_callback,
                                        unsigned int token_count,
                                        const std::string& description,
                                        const std::string& short_name,
                                        bool composing,
                                        bool positional,
                                        bool last_option)
{
    // Compose the name for boost::po. 
    // It takes the format "long_name, short name"
    const std::string comma(",");
    std::string boost_option_name = option_name;
    if(short_name != LLStringUtil::null)
    {
        boost_option_name += comma;
        boost_option_name += short_name;
    }
   
    LLCLPValue* value_desc = new LLCLPValue();
    value_desc->setMinTokens(token_count);
    value_desc->setMaxTokens(token_count);
    value_desc->setComposing(composing);
    value_desc->setLastOption(last_option);

    if(!notify_callback.empty())
    {
        value_desc->setNotifyCallback(notify_callback);
    }
	
	po::options_description_easy_init gEasyInitDesc(&gOptionsDesc);
	gEasyInitDesc(boost_option_name.c_str(),value_desc, description.c_str());

    if(positional)
    {
        gPositionalOptions.add(boost_option_name.c_str(), token_count);
    }
}

bool LLCommandLineParser::parseAndStoreResults(po::command_line_parser& clp)
{
    try
    {
        clp.options(gOptionsDesc);
        clp.positional(gPositionalOptions);
		// SNOW-626: Boost 1.42 erroneously added allow_guessing to the default style
		// (see http://groups.google.com/group/boost-list/browse_thread/thread/545d7bf98ff9bb16?fwc=2&pli=1)
		// Remove allow_guessing from the default style, because that is not allowed
		// when we have options that are a prefix of other options (aka, --help and --helperuri).
        clp.style((po::command_line_style::default_style & ~po::command_line_style::allow_guessing)
                  | po::command_line_style::allow_long_disguise);
		if(mExtraParser)
		{
			clp.extra_parser(mExtraParser);
		}
			
        po::basic_parsed_options<char> opts = clp.run();
        po::store(opts, gVariableMap);
    }
    catch (const po::error& e)
    {
        LL_WARNS() << "Caught Error:" << e.what() << LL_ENDL;
		mErrorMsg = e.what();
        return false;
    }
    catch (const LLCLPError& e)
    {
        LL_WARNS() << "Caught Error:" << e.what() << LL_ENDL;
		mErrorMsg = e.what();
        return false;
    }
    catch (const LLCLPLastOption&)
    {
		// This exception means a token was read after an option 
		// that must be the last option was reached (see url and slurl options)

        // boost::po will have stored a malformed option. 
        // All such options will be removed below.
		// The last option read, the last_option option, and its value
		// are put into the error message.
		std::string last_option;
		std::string last_value;
        for(po::variables_map::iterator i = gVariableMap.begin(); i != gVariableMap.end();)
        {
            po::variables_map::iterator tempI = i++;
            if(tempI->second.empty())
            {
                gVariableMap.erase(tempI);
            }
			else
			{
				last_option = tempI->first;
		        LLCommandLineParser::token_vector_t* tv = 
				    boost::any_cast<LLCommandLineParser::token_vector_t>(&(tempI->second.value())); 
				if(!tv->empty())
				{
					last_value = (*tv)[tv->size()-1];
				}
			}
        }

		// Continue without parsing.
		std::ostringstream msg;
		msg << "Caught Error: Found options after last option: " 
			<< last_option << " "
			<< last_value;

        LL_WARNS() << msg.str() << LL_ENDL;
		mErrorMsg = msg.str();
        return false;
    } 
    return true;
}

bool LLCommandLineParser::parseCommandLine(int argc, char **argv)
{
    po::command_line_parser clp(argc, argv);
    return parseAndStoreResults(clp);
}

// TODO:
// - Break out this funky parsing logic into separate method
// - Unit-test it with tests like LLStringUtil::getTokens() (the command-line
//   overload that supports quoted tokens)
// - Unless this logic offers significant semantic benefits, replace it with
//   LLStringUtil::getTokens(). This would fix a known bug: you cannot --set a
//   string-valued variable to the empty string, because empty strings are
//   eliminated below.

bool LLCommandLineParser::parseCommandLineString(const std::string& str)
{
    std::string cmd_line_string("");
    if (!str.empty())
    {
        bool add_last_c = true;
        S32 last_c_pos = str.size() - 1; //don't get out of bounds on pos+1, last char will be processed separately
        for (S32 pos = 0; pos < last_c_pos; ++pos)
        {
            cmd_line_string.append(&str[pos], 1);
            if (str[pos] == '\\')
            {
                cmd_line_string.append("\\", 1);
                if (str[pos + 1] == '\\')
                {
                    ++pos;
                    add_last_c = (pos != last_c_pos);
                }
            }
        }
        if (add_last_c)
        {
            cmd_line_string.append(&str[last_c_pos], 1);
            if (str[last_c_pos] == '\\')
            {
                cmd_line_string.append("\\", 1);
            }
        }
    }

    // Split the string content into tokens
    const char* escape_chars = "\\";
    const char* separator_chars = "\r\n ";
    const char* quote_chars = "\"'";
    boost::escaped_list_separator<char> sep(escape_chars, separator_chars, quote_chars);
    boost::tokenizer< boost::escaped_list_separator<char> > tok(cmd_line_string, sep);
    std::vector<std::string> tokens;
    // std::copy(tok.begin(), tok.end(), std::back_inserter(tokens));
    for(boost::tokenizer< boost::escaped_list_separator<char> >::iterator i = tok.begin();
        i != tok.end();
        ++i)
    {
        if(!i->empty())
        {
            tokens.push_back(*i);
        }
    }

    po::command_line_parser clp(tokens);
    return parseAndStoreResults(clp);
        
}

bool LLCommandLineParser::parseCommandLineFile(const std::basic_istream < char >& file)
{
    std::string args;
    read_file_into_string(args, file);

    return parseCommandLineString(args);
}

void LLCommandLineParser::notify()
{
    try
    {
        po::notify(gVariableMap);
    }
    catch (const LLCLPError& e)
    {
        LL_WARNS() << "Caught Error: " << e.what() << LL_ENDL;
        mErrorMsg = e.what();
    }
}

void LLCommandLineParser::printOptions() const
{
    for (auto& i : gVariableMap)
    {
        std::string name = i.first;
        token_vector_t values = i.second.as<token_vector_t>();
        std::ostringstream oss;
        oss << name << ": ";
        for (auto& value : values)
        {
            oss << value.c_str() << " ";
        }
        LL_INFOS() << oss.str() << LL_ENDL;
    }
}

std::ostream& LLCommandLineParser::printOptionsDesc(std::ostream& os) const
{
    return os << gOptionsDesc;
}

bool LLCommandLineParser::hasOption(const std::string& name) const
{
    return gVariableMap.count(name) > 0;
}

const LLCommandLineParser::token_vector_t& LLCommandLineParser::getOption(const std::string& name) const
{
    if(hasOption(name))
    {
        return gVariableMap[name].as<token_vector_t>();
    }

    return gEmptyValue;
}

//----------------------------------------------------------------------------
// LLControlGroupCLP defintions
//----------------------------------------------------------------------------
void setControlValueCB(const LLCommandLineParser::token_vector_t& value, 
                       const std::string& opt_name, 
                       LLControlGroup* ctrlGroup)
{
    // *FIX: Do sematic conversion here.
    // LLSD (ImplString) Is no good for doing string to type conversion for...
    // booleans
    // compound types
    // ?...

    LLControlVariable* ctrl = ctrlGroup->getControl(opt_name);
    if(NULL != ctrl)
    {
        switch(ctrl->type())
        {
        case TYPE_BOOLEAN:
            if(value.size() > 1)
            {
                LL_WARNS() << "Ignoring extra tokens." << LL_ENDL; 
            }
              
            if(value.size() > 0)
            {
                // There's a token. check the string for true/false/1/0 etc.
                BOOL result = false;
                BOOL gotSet = LLStringUtil::convertToBOOL(value[0], result);
                if(gotSet)
                {
                    ctrl->setValue(LLSD(result), false);
                }
            }
            else
            {
                ctrl->setValue(LLSD(true), false);
            }
            break;

        default:
            {
                // For the default types, let llsd do the conversion.
                if(value.size() > 1 && ctrl->isType(TYPE_LLSD))
                {
                    // Assume its an array...
                    LLSD llsdArray;
                    for(unsigned int i = 0; i < value.size(); ++i)
                    {
                        LLSD llsdValue;
                        llsdValue.assign(LLSD::String(value[i]));
                        llsdArray.set(i, llsdValue);
                    }

                    ctrl->setValue(llsdArray, false);
                }
                else if(value.size() > 0)
                {
					if(value.size() > 1)
					{
						LL_WARNS() << "Ignoring extra tokens mapped to the setting: " << opt_name << "." << LL_ENDL; 
					}

                    LLSD llsdValue;
                    llsdValue.assign(LLSD::String(value[0]));
                    ctrl->setValue(llsdValue, false);
                }
            }
            break;
        }
    }
    else
    {
        LL_WARNS() << "Command Line option mapping '" 
            << opt_name 
            << "' not found! Ignoring." 
            << LL_ENDL;
    }
}

void LLControlGroupCLP::configure(const std::string& config_filename, LLControlGroup* controlGroup)
{
    // This method reads the llsd based config file, and uses it to set 
    // members of a control group.
    LLSD clpConfigLLSD;
    
    llifstream input_stream;
    input_stream.open(config_filename, std::ios::in | std::ios::binary);

    if(input_stream.is_open())
    {
        LLSDSerialize::fromXML(clpConfigLLSD, input_stream);
        for(LLSD::map_iterator option_itr = clpConfigLLSD.beginMap(); 
            option_itr != clpConfigLLSD.endMap(); 
            ++option_itr)
        {
            LLSD::String long_name = option_itr->first;
            LLSD option_params = option_itr->second;
            
            std::string desc("n/a");
            if(option_params.has("desc"))
            {
                desc = option_params["desc"].asString();
            }
            
            std::string short_name = LLStringUtil::null;
            if(option_params.has("short"))
            {
                short_name = option_params["short"].asString();
            }

            unsigned int token_count = 0;
            if(option_params.has("count"))
            {
                token_count = option_params["count"].asInteger();
            }

            bool composing = false;
            if(option_params.has("compose"))
            {
                composing = option_params["compose"].asBoolean();
            }

            bool positional = false;
            if(option_params.has("positional"))
            {
                positional = option_params["positional"].asBoolean();
            }

            bool last_option = false;
            if(option_params.has("last_option"))
            {
                last_option = option_params["last_option"].asBoolean();
            }

            boost::function1<void, const token_vector_t&> callback;
            if(option_params.has("map-to") && (NULL != controlGroup))
            {
                std::string controlName = option_params["map-to"].asString();
                callback = boost::bind(setControlValueCB, _1, 
                                       controlName, controlGroup);
            }

            this->addOptionDesc(
                long_name, 
                callback,
                token_count, 
                desc, 
                short_name, 
                composing,
                positional,
                last_option);
        }
    }
}
