#pragma once

#define SCHEME "fox"
#define DOMAIN "base"
#define SCHEME_OPTIONS                                    \
  CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | \
      CEF_SCHEME_OPTION_CORS_ENABLED | CEF_SCHEME_OPTION_FETCH_ENABLED
