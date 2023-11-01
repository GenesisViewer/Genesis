# -*- cmake -*-

include(Python)

macro (check_message_template _target)
  add_custom_command(
      TARGET ${_target}
      PRE_LINK
      COMMAND ${PYTHON_EXECUTABLE}
      ARGS ${SCRIPTS_DIR}/template_verifier.py
           --mode=development --cache_master --master_url="https://github.com/secondlife/master-message-template/raw/master/message_template.msg" ${TEMPLATE_VERIFIER_OPTIONS}
      COMMENT "Verifying message template - See http://wiki.secondlife.com/wiki/Template_verifier.py"
      )
endmacro (check_message_template)
