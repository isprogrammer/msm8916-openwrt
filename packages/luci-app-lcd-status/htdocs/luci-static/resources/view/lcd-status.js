// SPDX-License-Identifier: GPL-2.0-only
'use strict';
'require view';
'require form';
'require uci';

function normalizeColor(value) {
	var v = (value || '').trim().replace(/^#/, '');
	if (/^[0-9a-f]{3}$/i.test(v))
		v = v.split('').map(function(c) { return c + c; }).join('');
	return /^[0-9a-f]{6}$/i.test(v) ? '#' + v.toLowerCase() : null;
}

function colorValue(value, fallback) {
	return normalizeColor(value) || normalizeColor(fallback) || '#000000';
}

function fieldValue(root, id, fallback) {
	var modal = document.querySelector('body.modal-overlay-active > #modal_overlay .cbi-map:not(.hidden)');
	var el = modal ? modal.querySelector('[id="' + CSS.escape('widget.' + id) + '"], [id="' + CSS.escape(id) + '"]') : null;
	if (!el) el = root.querySelector('#' + CSS.escape('widget.' + id));
	if (!el) el = root.querySelector('#' + CSS.escape(id));
	return el ? el.value : fallback;
}

function setupColors(scope, drawRoot) {
	drawRoot = drawRoot || scope;
	var inputs = scope.querySelectorAll('input[id^="widget.cbid.lcdstatus."], input[id^="cbid.lcdstatus."]');
	Array.prototype.forEach.call(inputs, function(input) {
		if (!/(\.fg|\.bg|\.color)$/.test(input.id) || input.dataset.colorSwatch) return;
		var frame = input.parentNode;
		if (!frame || !frame.parentNode) return;
		input.dataset.colorSwatch = '1';
		var controls = E('span', { 'class': 'lcdstatus-color-controls' });
		controls.style.cssText = 'display:inline-flex;align-items:center;gap:6px;max-width:100%';
		var picker = E('input', { type: 'color', 'aria-label': _('Choose color'), title: _('Choose color') });
		picker.style.cssText = 'display:inline-block;box-sizing:border-box;width:32px;height:24px;padding:0;border:1px solid #888;vertical-align:middle;flex:0 0 32px;cursor:pointer';
		var lastValid = normalizeColor(input.value) || '#808080';
		function sync() {
			var color = normalizeColor(input.value);
			if (color) lastValid = color;
			else color = lastValid;
			picker.value = color;
		}
		function chooseColor(ev) {
			ev.stopPropagation();
			var color = normalizeColor(picker.value);
			if (!color) return;
			lastValid = color;
			picker.value = color;
			input.value = color.slice(1).toUpperCase();
			['input', 'keyup', 'change', 'blur'].forEach(function(type) {
				input.dispatchEvent(new Event(type, { bubbles: true }));
			});
			if (drawRoot.__lcdStatusDraw) drawRoot.__lcdStatusDraw();
		}
		input.addEventListener('input', sync);
		input.addEventListener('change', sync);
		input.addEventListener('keyup', sync);
		if (drawRoot !== scope && !drawRoot.contains(input)) {
			input.addEventListener('input', function() { if (drawRoot.__lcdStatusDraw) drawRoot.__lcdStatusDraw(); });
			input.addEventListener('change', function() { if (drawRoot.__lcdStatusDraw) drawRoot.__lcdStatusDraw(); });
		}
		picker.addEventListener('input', chooseColor);
		picker.addEventListener('change', chooseColor);
		frame.parentNode.insertBefore(controls, frame);
		controls.appendChild(frame);
		controls.appendChild(picker);
		sync();
	});
}

function setupPreview(root) {
	var firstFrame = root.querySelector('[id="cbid.lcdstatus.main.enabled"], [id="widget.cbid.lcdstatus.main.enabled"]');
	if (!firstFrame) return;

	var canvas = root.querySelector('canvas[data-lcdstatus-preview]');
	if (!canvas) {
		var section = firstFrame.closest('.cbi-section-node') || firstFrame.closest('.cbi-section') || firstFrame.parentNode;
		var shell = E('div', { 'data-lcdstatus-shell': '1', style: 'display:flex;gap:24px;align-items:flex-start;flex-wrap:wrap' });
		var formSide = E('div', { style: 'flex:1 1 480px;min-width:320px' });
		var previewSide = E('div', { style: 'flex:1 1 320px;display:flex;flex-direction:column;align-items:center;text-align:center;position:sticky;top:1em' });
		canvas = E('canvas', { width: 128, height: 128, title: _('LCD preview'), 'data-lcdstatus-preview': '1' });
		canvas.style.cssText = 'width:300px;height:300px;image-rendering:pixelated;border:1px solid #888;background:#000';
		previewSide.appendChild(E('div', { 'class': 'cbi-value-title', text: _('Live preview') }));
		previewSide.appendChild(canvas);
		section.parentNode.insertBefore(shell, section);
		shell.appendChild(formSide);
		shell.appendChild(previewSide);
		formSide.appendChild(section);
	}

	var ctx = canvas.getContext('2d');
	var samples = { time: '12:34:56', uptime: '2d 04h 15m', lan_ip: '192.168.1.1', wan_ip: '10.0.0.2', wifi_clients: '3', cpu_load: '0.18', mem_free: '42 MB', battery: '78%', power_source: 'USB/AC', estimated_remaining: '5h 20m', exec: 'sample' };
	function draw() {
		var bg = colorValue(fieldValue(root, 'cbid.lcdstatus.main.bg', '000000'), '000000');
		var fg = colorValue(fieldValue(root, 'cbid.lcdstatus.main.fg', 'FFFFFF'), 'FFFFFF');
		var scale = fieldValue(root, 'cbid.lcdstatus.main.font_scale', '1') === '2' ? 2 : 1;
		var rotation = fieldValue(root, 'cbid.lcdstatus.main.rotation', '0') === '180';
		ctx.fillStyle = bg; ctx.fillRect(0, 0, 128, 128);
		if (rotation) { ctx.save(); ctx.translate(128, 128); ctx.rotate(Math.PI); }
		ctx.font = (8 * scale) + 'px monospace'; ctx.textBaseline = 'top';
		var labels = root.querySelectorAll('input[id^="widget.cbid.lcdstatus."][id$=".label"], input[id^="cbid.lcdstatus."][id$=".label"]');
		Array.prototype.forEach.call(labels, function(label, n) {
			var cbid = label.id.replace(/^widget\./, '').slice(0, -6);
			var sid = cbid.substring('cbid.lcdstatus.'.length);
			var src = fieldValue(root, cbid + '.source', null) || 'time';
			var cmd = src === 'exec' ? fieldValue(root, cbid + '.command', null) : null;
			if (src === 'exec' && cmd == null) cmd = uci.get('lcdstatus', sid, 'command');
			var lineColor = fieldValue(root, cbid + '.color', null);
			var value = src === 'exec' && cmd ? cmd : (samples[src] || '--');
			var text = ((label.value || '') ? label.value + ': ' : '') + value;
			var color = colorValue(lineColor, fg);
			ctx.fillStyle = color; ctx.fillText(text.toUpperCase().slice(0, Math.floor(128 / (6 * scale))), 0, n * 10 * scale);
		});
		if (rotation) ctx.restore();
	}
	root.__lcdStatusDraw = draw;
	if (!root.__lcdStatusEventsAttached) {
		['input', 'change'].forEach(function(type) {
			root.addEventListener(type, function() {
				if (root.__lcdStatusDraw) root.__lcdStatusDraw();
			});
		});
		root.__lcdStatusEventsAttached = true;
	}
	draw();
}

function setupMapEnhancements(root) {
	setupColors(root, root);
	setupPreview(root);
	if (!root.__lcdStatusObserver) {
		root.__lcdStatusWasConnected = !!root.isConnected;
		root.__lcdStatusObserver = new MutationObserver(function() {
			if (root.__lcdStatusWasConnected && !root.isConnected) {
				root.__lcdStatusObserver.disconnect();
				return;
			}
			if (root.isConnected) root.__lcdStatusWasConnected = true;
			setupColors(document.body, root);
			setupPreview(root);
		});
		root.__lcdStatusObserver.observe(document.body, { childList: true, subtree: true });
	}
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
		var s = m.section(form.GridSection, 'line', _('Display lines')); s.anonymous = true; s.addremove = true; s.sortable = true; s.description = _('Lines are rendered in order from top to bottom.');
		o = s.option(form.Value, 'label', _('Label')); o.maxlength = 31; o.editable = true;
		o = s.option(form.ListValue, 'source', _('Value source')); o.editable = true;
		['time', 'uptime', 'lan_ip', 'wan_ip', 'wifi_clients', 'cpu_load', 'mem_free', 'battery', 'power_source', 'estimated_remaining', 'exec'].forEach(function(v) { o.value(v, v.replace(/_/g, ' ')); }); o.default = 'time';
		o = s.option(form.Value, 'command', _('Custom command')); o.depends('source', 'exec'); o.maxlength = 255; o.description = _('Runs through the system shell with a 2.5 second timeout.'); o.modalonly = true;
		o = s.option(form.Value, 'color', _('Line color (RGB hex)')); o.datatype = 'and(hexstring,rangelength(6,6))'; o.default = 'FFFFFF'; o.editable = true;
		return m.render().then(function(node) { setupMapEnhancements(node); return node; });
	}
});
