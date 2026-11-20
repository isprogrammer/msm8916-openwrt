// SPDX-License-Identifier: GPL-2.0-only
'use strict';
'require view';
'require form';

function colorValue(value, fallback) {
	var v = (value || fallback || '000000').replace(/^#/, '');
	return /^[0-9a-fA-F]{6}$/.test(v) ? '#' + v : '#' + fallback;
}

function fieldValue(id, fallback) {
	var el = document.getElementById(id);
	return el ? el.value : fallback;
}

function setupColors(root) {
	var inputs = root.querySelectorAll('input[id^="cbid.lcdstatus."]');
	function contrastBorder() {
		var bg = window.getComputedStyle(document.body).backgroundColor;
		var m = bg.match(/[\d.]+/g), lum = 1;
		if (m && m.length >= 3) {
			var c = m.slice(0, 3).map(function(x) { return Number(x) / 255; });
			var lin = c.map(function(x) { return x <= 0.04045 ? x / 12.92 : Math.pow((x + 0.055) / 1.055, 2.4); });
			lum = 0.2126 * lin[0] + 0.7152 * lin[1] + 0.0722 * lin[2];
		}
		return lum > 0.5 ? '#000000' : '#ffffff';
	}
	Array.prototype.forEach.call(inputs, function(input) {
		if (!/(\.fg|\.bg|\.color)$/.test(input.id) || input.dataset.colorSwatch) return;
		input.dataset.colorSwatch = '1';
		var swatch = E('button', { 'class': 'btn lcdstatus-color-swatch', type: 'button', title: _('Choose color') });
		swatch.style.cssText = 'width:24px;height:24px;padding:2px;margin-right:6px;vertical-align:middle';
		var picker = E('input', { type: 'color', 'aria-label': _('Choose color') });
		picker.style.cssText = 'position:absolute;width:1px;height:1px;opacity:0;pointer-events:none';
		function sync() {
			var color = colorValue(input.value, 'FFFFFF');
			swatch.style.backgroundColor = color;
			swatch.style.border = '2px solid ' + contrastBorder();
			picker.value = color;
		}
		swatch.addEventListener('click', function() { picker.click(); });
		picker.addEventListener('input', function() { input.value = picker.value.slice(1).toUpperCase(); input.dispatchEvent(new Event('input', { bubbles: true })); });
		picker.addEventListener('change', function() { input.dispatchEvent(new Event('change', { bubbles: true })); });
		input.addEventListener('input', sync); input.addEventListener('change', sync);
		input.parentNode.insertBefore(swatch, input); input.parentNode.insertBefore(picker, input);
		sync();
	});
	new MutationObserver(function() { inputs.forEach(function(i) { var b = i.parentNode.querySelector('.lcdstatus-color-swatch'); if (b) b.style.borderColor = contrastBorder(); }); }).observe(document.documentElement, { attributes: true, subtree: true, attributeFilter: ['class', 'data-theme'] });
}

function setupPreview(root) {
	var first = document.getElementById('cbid.lcdstatus.main.enabled');
	if (!first) return;
	var section = first.closest('.cbi-section-node') || first.closest('.cbi-section') || first.parentNode;
	var shell = E('div', { style: 'display:flex;gap:24px;align-items:flex-start;flex-wrap:wrap' });
	var formSide = E('div', { style: 'flex:1 1 560px;min-width:320px' });
	var previewSide = E('div', { style: 'flex:0 0 180px;text-align:center;position:sticky;top:1em' });
	var canvas = E('canvas', { width: 128, height: 128, title: _('LCD preview') });
	canvas.style.cssText = 'width:160px;height:160px;image-rendering:pixelated;border:1px solid #888;background:#000';
	previewSide.appendChild(E('div', { 'class': 'cbi-value-title', text: _('Live preview') }));
	previewSide.appendChild(canvas);
	section.parentNode.insertBefore(shell, section); shell.appendChild(formSide); shell.appendChild(previewSide); formSide.appendChild(section);
	var ctx = canvas.getContext('2d');
	var samples = { time: '12:34:56', uptime: '2d 04h 15m', lan_ip: '192.168.1.1', wan_ip: '10.0.0.2', wifi_clients: '3', cpu_load: '0.18', mem_free: '42 MB', battery: '78%', power_source: 'USB/AC', estimated_remaining: '5h 20m', exec: 'sample' };
	function draw() {
		var bg = colorValue(fieldValue('cbid.lcdstatus.main.bg', '000000'), '000000');
		var fg = colorValue(fieldValue('cbid.lcdstatus.main.fg', 'FFFFFF'), 'FFFFFF');
		var scale = fieldValue('cbid.lcdstatus.main.font_scale', '1') === '2' ? 2 : 1;
		var rotation = fieldValue('cbid.lcdstatus.main.rotation', '0') === '180';
		ctx.fillStyle = bg; ctx.fillRect(0, 0, 128, 128);
		if (rotation) { ctx.save(); ctx.translate(128, 128); ctx.rotate(Math.PI); }
		ctx.font = (8 * scale) + 'px monospace'; ctx.textBaseline = 'top';
		var labels = root.querySelectorAll('input[id^="cbid.lcdstatus."][id$=".label"]');
		Array.prototype.forEach.call(labels, function(label, n) {
			var stem = label.id.slice(0, -6), source = root.querySelector('#' + CSS.escape(stem + 'source'));
			var colorInput = root.querySelector('#' + CSS.escape(stem + 'color'));
			var cmd = root.querySelector('#' + CSS.escape(stem + 'command'));
			var src = source ? source.value : 'time';
			var value = src === 'exec' && cmd && cmd.value ? cmd.value : (samples[src] || '--');
			var text = ((label.value || '') ? label.value + ': ' : '') + value;
			var color = colorInput ? colorValue(colorInput.value, 'FFFFFF') : fg;
			ctx.fillStyle = color; ctx.fillText(text.toUpperCase().slice(0, Math.floor(128 / (6 * scale))), 0, n * 10 * scale);
		});
		if (rotation) ctx.restore();
	}
	root.addEventListener('input', draw); root.addEventListener('change', draw); draw();
}

return view.extend({
	render: function() {
		var m = new form.Map('lcdstatus', _('LCD status display'), _('Configure the information shown on the attached framebuffer display. Preview updates before saving.'));
		var g = m.section(form.NamedSection, 'main', 'global', _('Display'));
		g.addremove = false;
		var o = g.option(form.Flag, 'enabled', _('Enabled')); o.default = '1'; o.rmempty = false;
		o = g.option(form.Value, 'refresh_s', _('Refresh interval (seconds)')); o.datatype = 'range(1,3600)'; o.default = '5';
		o = g.option(form.Value, 'auto_off_s', _('Auto-off after inactivity (seconds; 0 = never)')); o.datatype = 'range(0,86400)'; o.default = '0';
		o = g.option(form.ListValue, 'rotation', _('Rotation')); o.value('0', _('0 degrees')); o.value('180', _('180 degrees')); o.default = '0';
		o = g.option(form.Value, 'fg', _('Default foreground color (RGB hex)')); o.datatype = 'and(hexstring,rangelength(6,6))'; o.default = 'FFFFFF';
		o = g.option(form.Value, 'bg', _('Background color (RGB hex)')); o.datatype = 'and(hexstring,rangelength(6,6))'; o.default = '000000';
		o = g.option(form.ListValue, 'font_scale', _('Font scale')); o.value('1', _('Normal')); o.value('2', _('Double')); o.default = '1';
		o = g.option(form.Value, 'backlight_brightness', _('Backlight brightness (-1 leaves unchanged)')); o.datatype = 'integer'; o.default = '-1';
		var s = m.section(form.TypedSection, 'line', _('Display lines')); s.anonymous = true; s.addremove = true; s.sortable = true; s.description = _('Lines are rendered in order from top to bottom.');
		o = s.option(form.Value, 'label', _('Label')); o.maxlength = 31;
		o = s.option(form.ListValue, 'source', _('Value source'));
		['time', 'uptime', 'lan_ip', 'wan_ip', 'wifi_clients', 'cpu_load', 'mem_free', 'battery', 'power_source', 'estimated_remaining', 'exec'].forEach(function(v) { o.value(v, v.replace(/_/g, ' ')); }); o.default = 'time';
		o = s.option(form.Value, 'command', _('Custom command')); o.depends('source', 'exec'); o.maxlength = 255; o.description = _('Runs through the system shell with a 2.5 second timeout.');
		o = s.option(form.Value, 'color', _('Line color (RGB hex)')); o.datatype = 'and(hexstring,rangelength(6,6))'; o.default = 'FFFFFF';
		return m.render().then(function(node) { setupColors(node); setupPreview(node); return node; });
	}
});
