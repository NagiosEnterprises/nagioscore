<?php
// Login page for form based authentication (Apache mod_auth_form).
// See sample-config/httpd-formauth.conf for the matching Apache setup.
include_once(dirname(__FILE__).'/includes/utils.inc.php');

$this_version = '4.5.14';
// empty theme class: follow the operating system setting
$theme = isset($cfg['theme']) ? $cfg['theme'] : 'auto';
if ($theme != 'dark' && $theme != 'light') {
	$theme = '';
}

// Only allow redirects to local paths, never to another host.
$base = rtrim(dirname($_SERVER['SCRIPT_NAME']), '/') . '/';
$next = isset($_GET['next']) ? $_GET['next'] : '';
if (!is_string($next) || $next === '' || $next[0] !== '/' || strpos($next, '//') === 0
		|| strpbrk($next, "\\\r\n") !== false || strpos($next, $base . 'login.php') === 0) {
	$next = $base;
}

$message = '';
$message_class = '';
if (isset($_GET['error'])) {
	$message = 'Invalid username or password.';
	$message_class = 'error';
} elseif (isset($_GET['loggedout'])) {
	$message = 'You have been logged out.';
	$message_class = 'info';
}

header('X-Frame-Options: SAMEORIGIN');
header('Cache-Control: no-store');
?>
<!DOCTYPE html>
<html lang="en" class="<?= $theme ?>">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="ROBOTS" content="NOINDEX, NOFOLLOW">
<meta name="color-scheme" content="<?= $theme ? $theme : 'light dark' ?>">
<title>Login - Nagios Core</title>
<link rel="icon" href="images/favicon.ico">
<script>
	// The UI is a frameset, so an expired session would show this page inside
	// a single frame. Move it to the top window and return there after login.
	if (window.top !== window.self) {
		var t = window.top.location;
		window.top.location.href = window.location.pathname +
			'?next=' + encodeURIComponent(t.pathname + t.search);
	}
</script>
<style>
	:root {
		--bg: #f3f5f8;
		--card: rgba(255, 255, 255, 0.8);
		--card-border: rgba(15, 23, 42, 0.08);
		--text: #111827;
		--muted: #6b7280;
		--input-bg: #fff;
		--input-border: #d6dbe2;
		--grid: rgba(15, 23, 42, 0.04);
		--logo: #000;
	}
	@media (prefers-color-scheme: dark) {
		:root:not(.light) {
			--bg: #07090c;
			--card: rgba(22, 26, 32, 0.72);
			--card-border: rgba(255, 255, 255, 0.08);
			--text: #e8ebef;
			--muted: #8a93a0;
			--input-bg: rgba(0, 0, 0, 0.35);
			--input-border: rgba(255, 255, 255, 0.12);
			--grid: rgba(255, 255, 255, 0.035);
			--logo: #fff;
		}
	}
	:root.dark {
		--bg: #07090c;
		--card: rgba(22, 26, 32, 0.72);
		--card-border: rgba(255, 255, 255, 0.08);
		--text: #e8ebef;
		--muted: #8a93a0;
		--input-bg: rgba(0, 0, 0, 0.35);
		--input-border: rgba(255, 255, 255, 0.12);
		--grid: rgba(255, 255, 255, 0.035);
		--logo: #fff;
	}
	:root {
		--ok: #3ecf5a;
		--warning: #f5b53d;
		--critical: #f0524d;
		--accent: #3ecf5a;
		--accent-dark: #22a33c;
	}
	* { box-sizing: border-box; }
	html, body { min-height: 100%; }
	body {
		margin: 0;
		padding: 24px 16px;
		min-height: 100vh;
		min-height: 100dvh;
		display: flex;
		flex-direction: column;
		align-items: center;
		justify-content: center;
		background:
			radial-gradient(600px circle at 15% 10%, rgba(62, 207, 90, 0.14), transparent 60%),
			radial-gradient(500px circle at 85% 85%, rgba(240, 82, 77, 0.10), transparent 60%),
			radial-gradient(400px circle at 90% 15%, rgba(245, 181, 61, 0.08), transparent 60%),
			linear-gradient(var(--grid) 1px, transparent 1px) 0 0 / 32px 32px,
			linear-gradient(90deg, var(--grid) 1px, transparent 1px) 0 0 / 32px 32px,
			var(--bg);
		color: var(--text);
		font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, verdana, arial, sans-serif;
		font-size: 15px;
		line-height: 1.45;
		-webkit-font-smoothing: antialiased;
	}
	.login {
		position: relative;
		width: 100%;
		max-width: 380px;
		padding: 36px 32px 30px;
		background: var(--card);
		border: 1px solid var(--card-border);
		border-radius: 16px;
		box-shadow: 0 24px 60px -20px rgba(0, 0, 0, 0.55), 0 0 0 1px rgba(62, 207, 90, 0.04);
		-webkit-backdrop-filter: blur(16px);
		backdrop-filter: blur(16px);
		animation: rise .5s cubic-bezier(.2, .8, .2, 1) both;
	}
	.login::before {
		content: "";
		position: absolute;
		top: 0; left: 24px; right: 24px;
		height: 1px;
		background: linear-gradient(90deg, transparent, var(--ok), var(--warning), var(--critical), transparent);
		opacity: .8;
	}
	.logo {
		mask: url(images/logos/horizontal-nagios-full-logo.svg) no-repeat center / contain;
		-webkit-mask: url(images/logos/horizontal-nagios-full-logo.svg) no-repeat center / contain;
		background-color: var(--logo);
		height: 42px;
		margin: 0 auto 14px;
	}
	.status {
		display: flex;
		justify-content: center;
		gap: 8px;
		margin: 0 0 22px;
	}
	.status span {
		width: 8px;
		height: 8px;
		border-radius: 50%;
		animation: pulse 2.4s ease-in-out infinite;
	}
	.status span:nth-child(1) { background: var(--ok); box-shadow: 0 0 10px var(--ok); }
	.status span:nth-child(2) { background: var(--warning); box-shadow: 0 0 10px var(--warning); animation-delay: .3s; }
	.status span:nth-child(3) { background: var(--critical); box-shadow: 0 0 10px var(--critical); animation-delay: .6s; }
	h1 {
		margin: 0 0 4px;
		text-align: center;
		font-size: 20px;
		font-weight: 600;
		letter-spacing: -0.01em;
	}
	.subtitle {
		margin: 0 0 26px;
		text-align: center;
		font-size: 13px;
		color: var(--muted);
	}
	label {
		display: block;
		margin: 0 0 6px;
		font-size: 12px;
		font-weight: 600;
		letter-spacing: .04em;
		text-transform: uppercase;
		color: var(--muted);
	}
	.field {
		position: relative;
		margin: 0 0 18px;
	}
	.field svg {
		position: absolute;
		left: 13px;
		top: 50%;
		width: 18px;
		height: 18px;
		transform: translateY(-50%);
		stroke: var(--muted);
		pointer-events: none;
	}
	input[type=text], input[type=password] {
		display: block;
		width: 100%;
		height: 48px;
		padding: 0 14px 0 42px;
		/* 16px keeps iOS Safari from zooming into the field */
		font-size: 16px;
		font-family: inherit;
		color: var(--text);
		background: var(--input-bg);
		border: 1px solid var(--input-border);
		border-radius: 10px;
		transition: border-color .15s, box-shadow .15s;
	}
	input[type=password], #password { padding-right: 48px; }
	input:focus {
		outline: none;
		border-color: var(--accent);
		box-shadow: 0 0 0 4px rgba(62, 207, 90, 0.18);
	}
	.toggle {
		position: absolute;
		right: 4px;
		top: 50%;
		transform: translateY(-50%);
		width: 40px;
		height: 40px;
		padding: 0;
		margin: 0;
		display: flex;
		align-items: center;
		justify-content: center;
		background: none;
		border: 0;
		border-radius: 8px;
		cursor: pointer;
	}
	.toggle svg { position: static; transform: none; }
	.toggle:hover svg, .toggle[aria-pressed=true] svg { stroke: var(--text); }
	.submit {
		width: 100%;
		height: 48px;
		margin-top: 8px;
		font-size: 16px;
		font-family: inherit;
		font-weight: 600;
		color: #04130a;
		background: linear-gradient(180deg, var(--accent), var(--accent-dark));
		border: 0;
		border-radius: 10px;
		box-shadow: 0 8px 20px -8px rgba(62, 207, 90, 0.6), inset 0 1px 0 rgba(255, 255, 255, 0.25);
		cursor: pointer;
		transition: transform .1s, filter .15s;
	}
	.submit:hover { filter: brightness(1.08); }
	.submit:active { transform: translateY(1px); }
	.submit:focus-visible { outline: 2px solid var(--text); outline-offset: 2px; }
	.message {
		display: flex;
		align-items: center;
		gap: 10px;
		margin: 0 0 20px;
		padding: 11px 14px;
		font-size: 14px;
		border-radius: 10px;
		border: 1px solid;
	}
	.message::before {
		content: "";
		flex: none;
		width: 8px;
		height: 8px;
		border-radius: 50%;
		background: currentColor;
	}
	.message.error {
		color: var(--critical);
		border-color: rgba(240, 82, 77, 0.35);
		background: rgba(240, 82, 77, 0.08);
	}
	.message.info {
		color: var(--ok);
		border-color: rgba(62, 207, 90, 0.35);
		background: rgba(62, 207, 90, 0.08);
	}
	.footer {
		margin: 22px 0 0;
		text-align: center;
		font-size: 12px;
		color: var(--muted);
	}
	@keyframes rise {
		from { opacity: 0; transform: translateY(12px); }
		to { opacity: 1; transform: none; }
	}
	@keyframes pulse {
		0%, 100% { opacity: 1; }
		50% { opacity: .35; }
	}
	@media (prefers-reduced-motion: reduce) {
		.login, .status span { animation: none; }
	}
	@media (max-width: 480px) {
		body { justify-content: flex-start; padding-top: max(10vh, 24px); }
		.login { padding: 28px 22px 24px; border-radius: 14px; }
	}
</style>
</head>
<body>
<main class="login">
	<div class="logo" role="img" aria-label="Nagios"></div>
	<div class="status" aria-hidden="true"><span></span><span></span><span></span></div>
	<h1>Welcome back</h1>
	<p class="subtitle">Sign in to Nagios Core</p>

	<?php if ($message !== '') { ?>
	<div class="message <?= $message_class ?>" role="alert"><?= htmlspecialchars($message) ?></div>
	<?php } ?>

	<form method="post" action="dologin">
		<label for="username">Username</label>
		<div class="field">
			<svg viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="8" r="4"/><path d="M4 21c0-4 4-6 8-6s8 2 8 6"/></svg>
			<input type="text" id="username" name="httpd_username" autocomplete="username"
				autocapitalize="none" autocorrect="off" spellcheck="false" required autofocus>
		</div>

		<label for="password">Password</label>
		<div class="field">
			<svg viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><rect x="4" y="11" width="16" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/></svg>
			<input type="password" id="password" name="httpd_password" autocomplete="current-password" required>
			<button type="button" class="toggle" id="toggle" aria-label="Show password" aria-pressed="false">
				<svg viewBox="0 0 24 24" fill="none" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/></svg>
			</button>
		</div>

		<input type="hidden" name="httpd_location" value="<?= htmlspecialchars($next) ?>">
		<button type="submit" class="submit">Log in</button>
	</form>

	<p class="footer">Nagios&reg; Core&trade; <?= $this_version ?></p>
</main>
<script>
	document.getElementById('toggle').addEventListener('click', function() {
		var pw = document.getElementById('password');
		var show = pw.type === 'password';
		pw.type = show ? 'text' : 'password';
		this.setAttribute('aria-pressed', show);
		this.setAttribute('aria-label', show ? 'Hide password' : 'Show password');
		pw.focus();
	});
</script>
</body>
</html>
