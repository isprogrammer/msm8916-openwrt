// SPDX-License-Identifier: GPL-2.0-only\n'use strict';
'require view';
'require form';

return view.extend({
	render: function() {
		var m = new form.Map('lcdstatus', _('LCD status display'),
			_('Configure the information shown on the attached framebuffer display.'));
		var g = m.section(form.NamedSection, 'main', 'global', _('Display'));
		g.addremove = false;

		var o = g.option(form.Flag, 'enabled', _('Enabled'));
		o.default = '1';
		o.rmempty = false;

		o = g.option(form.Value, 'refresh_s', _('Refresh interval (seconds)'));
		o.datatype = 'range(1,3600)';
		o.default = '5';

		o = g.option(form.ListValue, 'rotation', _('Rotation'));
		o.value('0', _('0 degrees'));
		o.value('180', _('180 degrees'));
		o.default = '0';

		o = g.option(form.Value, 'fg', _('Default foreground color (RGB hex)'));
		o.datatype = 'and(hexstring,rangelength(6,6))';
		o.default = 'FFFFFF';

		o = g.option(form.Value, 'bg', _('Background color (RGB hex)'));
		o.datatype = 'and(hexstring,rangelength(6,6))';
		o.default = '000000';

		o = g.option(form.ListValue, 'font_scale', _('Font scale'));
		o.value('1', _('Normal'));
		o.value('2', _('Double'));
		o.default = '1';

		o = g.option(form.Value, 'backlight_brightness', _('Backlight brightness (-1 leaves unchanged)'));
		o.datatype = 'integer';
		o.default = '-1';

		var s = m.section(form.TypedSection, 'line', _('Display lines'));
		s.anonymous = true;
		s.addremove = true;
		s.sortable = true;
		s.description = _('Lines are rendered in order from top to bottom.');

		o = s.option(form.Value, 'label', _('Label'));
		o.maxlength = 31;

		o = s.option(form.ListValue, 'source', _('Value source'));
		['time', 'uptime', 'lan_ip', 'wan_ip', 'wifi_clients',
		 'cpu_load', 'mem_free', 'battery', 'exec'].forEach(function(v) {
			o.value(v, v);
		});
		o.default = 'time';

		o = s.option(form.Value, 'command', _('Custom command'));
		o.depends('source', 'exec');
		o.maxlength = 255;
		o.description = _('Runs through the system shell with a 2.5 second timeout.');

		o = s.option(form.Value, 'color', _('Line color (RGB hex)'));
		o.datatype = 'and(hexstring,rangelength(6,6))';
		o.default = 'FFFFFF';

		return m.render();
	}
});
