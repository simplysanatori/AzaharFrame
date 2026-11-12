# Sourced by steam-frame/build.sh and build-portable.sh (AzaharFrame crashreport): the Sentry DSN that
# Send Report sends to. From AZAHARFRAME_SENTRY_DSN (CI: a GitHub secret), else the file
# AZAHARFRAME_SENTRY_DSN_FILE (default ~/.config/azaharframe-dev/sentry-dsn, one line). Never commit it:
# with none the build has no Send Report, and Save Report stays. Prints whether one was found, never the DSN.
if [ -z "${AZAHARFRAME_SENTRY_DSN:-}" ]; then
  _dsn_file=${AZAHARFRAME_SENTRY_DSN_FILE:-$HOME/.config/azaharframe-dev/sentry-dsn}
  if [ -r "$_dsn_file" ]; then
    AZAHARFRAME_SENTRY_DSN=$(head -n 1 "$_dsn_file" | tr -d '[:space:]')
  fi
  unset _dsn_file
fi
export AZAHARFRAME_SENTRY_DSN=${AZAHARFRAME_SENTRY_DSN:-}
if [ -n "$AZAHARFRAME_SENTRY_DSN" ]; then
  echo "crash reports: Send Report goes to the Sentry DSN given (AZAHARFRAME_SENTRY_DSN)"
else
  echo "crash reports: no Sentry DSN (AZAHARFRAME_SENTRY_DSN): Save Report only"
fi
