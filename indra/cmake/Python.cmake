# -*- cmake -*-

set(PYTHONINTERP_FOUND)

if (WINDOWS)
  # On Windows, explicitly avoid Cygwin Python.

  find_program(PYTHON_EXECUTABLE
    NAMES python.exe
    PATHS
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.10\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.11\\InstallPath]
    [HKEY_CURRENT_USER\\SOFTWARE\\Python\\PythonCore\\3.10\\InstallPath]
    [HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.11\\InstallPath]
    )
    include(FindPythonInterp)
else()
  find_program(PYTHON_EXECUTABLE python3)

  if (PYTHON_EXECUTABLE)
    set(PYTHONINTERP_FOUND ON)
  endif (PYTHON_EXECUTABLE)
endif (WINDOWS)

if (NOT PYTHON_EXECUTABLE)
  message(FATAL_ERROR "No Python interpreter found")
endif (NOT PYTHON_EXECUTABLE)

mark_as_advanced(PYTHON_EXECUTABLE)
