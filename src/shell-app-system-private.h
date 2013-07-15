/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
#ifndef __SHELL_APP_SYSTEM_PRIVATE_H__
#define __SHELL_APP_SYSTEM_PRIVATE_H__

#include "shell-app-system.h"

void _shell_app_system_notify_app_state_changed (ShellAppSystem *self, ShellApp *app);

/* from lowest to highest */
typedef enum {
  SHELL_MATCH_RANK_KEYWORD_SUBSTRING,
  SHELL_MATCH_RANK_KEYWORD_PREFIX,
  SHELL_MATCH_RANK_NAME_SUBSTRING,
  SHELL_MATCH_RANK_NAME_PREFIX,
  SHELL_MATCH_RANK_LAST
} ShellMatchRank;

#endif
