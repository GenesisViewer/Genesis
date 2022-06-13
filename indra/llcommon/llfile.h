/** 
 * @file llfile.h
 * @author Michael Schlachter
 * @date 2006-03-23
 * @brief Declaration of cross-platform POSIX file buffer and c++
 * stream classes.
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

#ifndef LL_LLFILE_H
#define LL_LLFILE_H

#include <fstream>
#include <sys/stat.h>

/**
 * This class provides a cross platform interface to the filesystem.
 * Attempts to mostly mirror the POSIX style IO functions.
 */

typedef FILE	LLFILE;

#if LL_WINDOWS
// windows version of stat function and stat data structure are called _stat
typedef struct _stat	llstat;
#else
typedef struct stat		llstat;
#include <sys/types.h>
#endif

#ifndef S_ISREG
# define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
# define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

#include "llstring.h" // safe char* -> std::string conversion

class LL_COMMON_API LLFile
{
public:
	// All these functions take UTF8 path/filenames.
	static	LLFILE*	fopen(const std::string& filename,const char* accessmode);	/* Flawfinder: ignore */
	static	LLFILE*	_fsopen(const std::string& filename,const char* accessmode,int	sharingFlag);

	static	int		close(LLFILE * file);

	// Singu extension: the same as below, but doesn't print a warning as to leave errno alone.
	static	int		mkdir_nowarn(const std::string& filename, int perms);
	static	int		rmdir_nowarn(const std::string& filename);
	static	int		remove_nowarn(const std::string& filename);
	static	int		rename_nowarn(const std::string& filename, const std::string& newname);

	// perms is a permissions mask like 0777 or 0700.  In most cases it will
	// be overridden by the user's umask.  It is ignored on Windows.
	static	int		mkdir(const std::string& filename, int perms = 0700);

	static	int		rmdir(const std::string& filename);
	static	int		remove(const std::string& filename, int supress_error = 0);
	static	int		rename(const std::string& filename,const std::string& newname, int supress_error = 0);
	static	int		stat(const std::string&	filename,llstat*	file_status);
	static	bool	isdir(const std::string&	filename);
	static	bool	isfile(const std::string&	filename);
	static	LLFILE *	_Fiopen(const std::string& filename, 
			std::ios::openmode mode);

	static  const char * tmpdir();

	static std::string strerr(int errn);
	static std::string strerr();
};

#if !defined(LL_WINDOWS)
typedef std::ifstream llifstream;
typedef std::ofstream llofstream;
#else

/**
 *  @brief  Controlling input for files.
 *
 *  This class supports reading from named files, using the inherited
 *  functions from std::ifstream. The only added value is that our constructor
 *  Does The Right Thing when passed a non-ASCII pathname. Sadly, that isn't
 *  true of Microsoft's std::ifstream.
 */
class LL_COMMON_API llifstream : public	std::ifstream
{
	// input stream associated with a C stream
public:
	// Constructors:
	/**
	 *  @brief  Default constructor.
	 *
	 *  Initializes @c sb using its default constructor, and passes
	 *  @c &sb to the base class initializer.  Does not open any files
	 *  (you haven't given it a filename to open).
	*/
	llifstream();

	/**
	 *  @brief  Create an input file stream.
	 *  @param  Filename  String specifying the filename.
	 *  @param  Mode  Open file in specified mode (see std::ios_base).
	 *
     *  @c ios_base::in is automatically included in @a mode.
	*/
	explicit llifstream(const std::string& _Filename,
			ios_base::openmode _Mode = ios_base::in);
	explicit llifstream(const char* _Filename,
			ios_base::openmode _Mode = ios_base::in);

	/**
	 *  @brief  Opens an external file.
	 *  @param  Filename  The name of the file.
	 *  @param  Node  The open mode flags.
	 *
	 *  Calls @c llstdio_filebuf::open(s,mode|in).  If that function
	 *  fails, @c failbit is set in the stream's error state.
	*/
	void open(const std::string& _Filename,
			ios_base::openmode _Mode = ios_base::in);
	void open(const char* _Filename,
			ios_base::openmode _Mode = ios_base::in);
};


/**
 *  @brief  Controlling output for files.
 *
 *  This class supports writing to named files, using the inherited functions
 *  from std::ofstream. The only added value is that our constructor Does The
 *  Right Thing when passed a non-ASCII pathname. Sadly, that isn't true of
 *  Microsoft's std::ofstream.
 */
class LL_COMMON_API llofstream : public	std::ofstream
{
public:
	// Constructors:
	/**
	 *  @brief  Default constructor.
	 *
	 *  Initializes @c sb using its default constructor, and passes
	 *  @c &sb to the base class initializer.  Does not open any files
	 *  (you haven't given it a filename to open).
	*/
	llofstream();

	/**
	 *  @brief  Create an output file stream.
	 *  @param  Filename  String specifying the filename.
	 *  @param  Mode  Open file in specified mode (see std::ios_base).
	 *
	 *  @c ios_base::out is automatically included in @a mode.
	*/
	explicit llofstream(const std::string& _Filename,
			ios_base::openmode _Mode = ios_base::out|ios_base::trunc);
	explicit llofstream(const char* _Filename,
			ios_base::openmode _Mode = ios_base::out|ios_base::trunc);

	/**
	 *  @brief  Opens an external file.
	 *  @param  Filename  The name of the file.
	 *  @param  Node  The open mode flags.
	 *
	 *  Calls @c llstdio_filebuf::open(s,mode|out).  If that function
	 *  fails, @c failbit is set in the stream's error state.
	*/
	void open(const std::string& _Filename,
			ios_base::openmode _Mode = ios_base::out | ios_base::trunc);
	void open(const char* _Filename,
			ios_base::openmode _Mode = ios_base::out|ios_base::trunc);
};
#endif

/**
 * @breif filesize helpers.
 *
 * The file size helpers are not considered particularly efficient,
 * and should only be used for config files and the like -- not in a
 * loop.
 */
std::streamsize LL_COMMON_API llifstream_size(llifstream& fstr);
std::streamsize LL_COMMON_API llofstream_size(llofstream& fstr);

#endif // not LL_LLFILE_H
