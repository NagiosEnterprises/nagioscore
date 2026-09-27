# Web UI redesign

The Nagios Core web UI got a modern, mobile friendly look with a dark and a
light theme, and an optional login form that password managers understand.
This document records what changed and what is still open.

## What changed

- **Form login** (optional): `html/login.php` and
  `sample-config/httpd-formauth.conf.in`, installed with
  `make install-webconf-formauth`. Apache `mod_auth_form` handles login,
  encrypted session cookie and logout (`/nagios/logout`); accounts still live
  in `htpasswd.users`. The install target generates a random
  `SessionCryptoPassphrase`.
- **Shell**: `index.php` replaces the two-frame layout with a sidebar
  (icons, host search, live problem badges, user, theme toggle) and a single
  content iframe. Below 900px the sidebar becomes a drawer with a top bar.
- **Theme**: follows the operating system by default (`$cfg['theme']='auto'`);
  `dark` / `light` force a theme. The sidebar toggle cycles
  auto → dark → light per browser.
- **Design system**: `common.css` defines all color tokens and shared
  components. Every page stylesheet was rewritten on top of it: status views,
  tactical overview, extended info (host, service, process, performance,
  comments, downtime, queue), outages, reports (availability, trends,
  history, summary, histogram, notifications, event log), commands,
  configuration, status map, JSON query tool and the home page.
- **Bug fixes found on the way**
  - `cgi/cmd.c`: the hidden `nagFormId` input lacked its closing `>`, which
    broke the DOM of every command form.
  - `index.php?corewindow=`: a leading `/` produced a protocol-relative
    `//nagios/...` URL, and query values were URL-encoded twice.

## Open issues

- **CGIs opened outside the shell are not mobile friendly.** CGI output has
  no doctype and no `<meta name="viewport">`, so a phone renders it 980px
  wide. Fixing this needs a shared HTML header function in `cgi/cgiutils.c`
  used by all CGIs (doctype, viewport, charset). Adding a doctype switches the
  pages from quirks to standards mode, so every page must be re-checked.
- **Status map on phones** scrolls horizontally. The map is a GD image with an
  image map; scaling the image would misplace the clickable areas.
- **Trends graph on phones** scrolls horizontally for the same reason (image
  map).
- **GD graphs in dark mode** (trends, histogram, status map) are server
  rendered on white. They are shown inverted/hue-rotated in dark mode, which
  keeps them readable but not pixel-perfect. Real dark graphs would need
  theme aware colors in the C code.
- **TAC tiles** show a status colored dot even when the count is 0; CSS
  cannot read the number. Needs a class for zero counts from `tac.c`.
- **Multi-line log entries** in history / event log have little spacing on
  phones; the lines are separated only by `<br>` in the CGI output.
- **Outage rows** (`outages.cgi`) are styled from the C source but were not
  seen with real data (no outage in the test setup).
- **Angular map (`map.php`) and `checksanity`** are not shipped by the
  default install; their stylesheets were only switched to the color tokens,
  not visually tested.
- **Legacy themes**: `make install-exfoliation` / `install-classicui` copy
  the old stylesheets over the new ones and undo the redesign. They should be
  dropped or reworked.
- **Line icons** replace the old GIF status icons via `content: url()` on
  `<img>`. Chrome, Safari and Firefox support it; other browsers fall back to
  the GIFs.
- **No automated UI tests.** The redesign was checked manually in Chrome at
  1280/1440px and 390px (mobile emulation), dark and light, with a demo
  configuration covering all host and service states. A small Docker based
  preview setup plus screenshot tests would make future changes safer.
