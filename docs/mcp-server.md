# MCP server

Nagios Core ships a [Model Context Protocol](https://modelcontextprotocol.io)
server, `mcp.cgi`. It lets AI assistants such as Claude query the monitoring
("what is broken right now?", "what happened to db-01 last night?") and act
on it (acknowledge, schedule downtime, re-check, comment) with the same
permissions a user has in the web interface.

- Transport: MCP *Streamable HTTP* in stateless mode. Each request is one
  JSON-RPC message sent with `POST`; the response is a single
  `application/json` body. There are no sessions and no extra daemon.
- Endpoint: `https://<server>/mcp` (the original CGI URL remains available).
- Public connection instructions: `https://<server>/mcp/help`.
- Protocol versions: 2025-11-25, 2025-06-18, 2025-03-26, 2024-11-05

## Setup

1. Build and install as usual. `make install` installs `mcp.cgi` with the
   other CGIs, and `make install-webconf` installs an Apache config that
   exempts `mcp.cgi` from password authentication and passes the
   `Authorization` header through (`CGIPassAuth On`, Apache 2.4.13+).
2. Create the first token on the server, as the Nagios user so the web
   server can read the token file:

   ```sh
   sudo -u nagios /usr/local/nagios/sbin/mcp.cgi --create-token \
       --user nagiosadmin --scopes admin --label "Claude Desktop (Jane)"
   ```

   The token (`nagmcp_…`) is printed once. Only its SHA-256 hash is stored,
   by default in `etc/mcp-tokens.cfg` next to `cgi.cfg`
   (`mcp_token_file` in `cgi.cfg` changes the location). The file must be
   readable and writable by the web server user (mode 0660, group nagios).
3. Connect the assistant, e.g. Claude Code:

   ```sh
   claude mcp add --transport http nagios https://nagios.example.com/mcp \
       --header "Authorization: Bearer nagmcp_…"
   ```

   Always use HTTPS: the token is a password.

## Permissions

A token acts as a Nagios user. Its effective rights are the intersection of

- the **user's rights** from `cgi.cfg` and contact membership (exactly as in
  the web interface: `authorized_for_*`, `authorized_for_read_only`,
  hosts/services the user is a contact for), and
- the **token's scopes**:

| Scope   | Adds |
|---------|------|
| `read`  | All read tools |
| `write` | Acknowledge, downtime, comments, check now, passive results, enable/disable notifications and checks |
| `admin` | Create/list/revoke tokens, run any external command (also needs `authorized_for_system_commands`) |
| `config` | Change the object configuration (see below). Not included in `admin`; needs `mcp_allow_config_changes=1`, `authorized_for_configuration_information` and `authorized_for_system_commands` |

A token can never do more than its user can do in the web interface. Tools
the token cannot use are not offered in `tools/list`, so the assistant does
not try them. Tokens can expire (`--expires-days`, `expires_in_days`).

Admins manage tokens for other users through MCP itself (`create_token`,
`list_tokens`, `revoke_token`) or on the command line:

```sh
mcp.cgi --create-token --user jane --scopes read,write --label "Jane laptop" --expires-days 90
mcp.cgi --list-tokens
mcp.cgi --revoke-token 72baf31a1c71
```

Token creation and revocation are logged to the web server's error log.
Commands submitted through MCP show up in the Nagios log like any external
command, with `<user> (MCP)` as author.

## Online help

Call `get_help` (optionally with `topic`: `monitoring`, `configuration`,
`authentication`, or `troubleshooting`) for workflows, examples and current
permission diagnostics. It is available to every authenticated read token,
even when configuration editing is disabled. Use `tools/list` for exact schemas.
The help never grants permissions or returns token secrets.

## Tools

**Read:** `get_help`, `get_overview`, `list_problems`, `list_hosts`, `get_host`,
`list_services`, `get_service`, `list_groups`, `list_comments`,
`list_downtimes`, `get_alert_history`, `get_notification_history`,
`get_availability`, `get_config`, `get_performance`

**Write:** `acknowledge_problem`, `remove_acknowledgement`,
`schedule_downtime`, `cancel_downtime`, `add_comment`, `delete_comment`,
`check_now`, `submit_check_result`, `set_notifications`,
`set_active_checks`

**Admin:** `run_external_command`, `create_token`, `list_tokens`,
`revoke_token`

**Config:** `list_config_files`, `get_config_source`, `plan_config_change`,
`apply_config_change`, `list_config_backups`, `restore_config_backup`

Each tool carries a description and a JSON Schema for its arguments
(`tools/list`). Times accept ISO 8601 (`2026-09-27T20:00:00Z`), unix
seconds, `now` or relative values like `-2h` and `+30m`.

## Changing the configuration

With the `config` scope an assistant can add, change and remove hosts,
services, host/service/contact groups, contacts, timeperiods and templates,
e.g. "I set up web-03 at 10.0.0.13, monitor SSH and HTTP":

1. `plan_config_change` computes the change without writing anything and
   returns a unified diff, the result of `nagios -v` on a staged copy of the
   changed configuration, and a `plan_id`.
2. The assistant shows the diff; after the user agrees,
   `apply_config_change` gets the same changes and the `plan_id`. It is
   refused if the files changed in between. The files are validated again,
   backed up (`list_config_backups`), written atomically and Nagios is
   asked to reload.
3. `restore_config_backup` undoes an applied change the same way (plan,
   then apply).

How files are edited:

- Existing objects are changed **in place in their file**: only the lines of
  their `define` block change, comments and formatting elsewhere stay as
  they are. The parser follows Nagios' own rules (`;` comments, `\;`,
  continuation lines with a trailing backslash).
- New objects go to `objects/mcp/<type>s.cfg` next to `nagios.cfg`
  (`mcp_config_dir`); a `cfg_dir` line is added to `nagios.cfg` if needed.
- Dependencies and escalations have no name and cannot be edited; objects
  defined more than once are refused.
- Command definitions are read-only unless `mcp_allow_command_changes=1`:
  a `command_line` runs programs on the server, so changing it is
  equivalent to shell access.
- Files the web server user cannot write are never replaced, even when the
  directory is writable. Give the web server write access to what it may
  change, e.g. `chgrp -R nagios objects && chmod -R g+w objects`.

Enable it in `cgi.cfg`:

```
mcp_allow_config_changes=1
#mcp_allow_command_changes=1   # only if you really want this
#mcp_nagios_binary=/usr/local/nagios/bin/nagios
```

and create a token with the scope: `mcp.cgi --create-token --user nagiosadmin --scopes admin,config`.

## How it works

- **Reading** delegates to `statusjson.cgi`, `objectjson.cgi` and
  `archivejson.cgi`, run as the token's user. Data access and authorization
  exist once, in the CGIs that already implement them. Results are trimmed to
  what an assistant needs (readable states, ISO dates).
- **Writing** submits external commands to the command file, with the same
  checks as `cmd.cgi` (read-only users, host/service/system command rights,
  no `CHANGE_*` commands). Fields cannot contain `;` or line breaks; free
  text has them replaced, so no second command can be injected.
- **Security:** bearer tokens only (the web server's `REMOTE_USER` is
  ignored), constant-time hash comparison, a foreign `Origin` header is
  rejected (DNS rebinding), request bodies are limited to 1 MB.

## Tests

- `cgi/test-mcputils.c`: C unit tests for the JSON parser/serializer,
  SHA-256, token file format, scopes, time parsing and field validation,
  using the `lib/t-utils.h` helpers.
- `cgi/test-mcpconfig.c`: C unit tests for the object file parser (comments,
  escapes, continuation lines, CRLF), the surgical edits and the diff.
  Run both with `cd cgi && make test`.
- `t/630mcp.t`: integration tests that run `mcp.cgi` like a web server
  against the fixture configuration: HTTP handling, protocol, every tool
  class, exact commands written to the command file, injection attempts,
  scopes, Nagios rights, token expiry and revocation, and the full
  configuration workflow (plan, validation with the real `nagios -v`,
  apply, surgical edits, read-only files, backups and restore).

Both run as part of `make test`.
