// SPDX-License-Identifier: GPL-2.0-only
'use strict';
'require view';
'require form';

return view.extend({
	render: function() {
		var m = new form.Map('pwrkey', _('Power button'), _('Configure the shared PMIC power-key actions. Defaults preserve the current behavior.'));
		var s = m.section(form.NamedSection, 'main', 'global', _('Power key actions'));
		s.addremove = false;
		var o = s.option(form.Flag, 'double_press_reboot', _('Double press reboots'));
		o.default = '1'; o.rmempty = false;
		o = s.option(form.Value, 'double_press_window_ms', _('Double-press window (milliseconds)'));
		o.datatype = 'range(100,10000)'; o.default = '800';
		o = s.option(form.ListValue, 'short_press_action', _('Short press action'));
		o.value('lcd-toggle', _('Toggle or wake LCD when available')); o.value('none', _('No action')); o.default = 'lcd-toggle';
		o = s.option(form.Value, 'long_hold_s', _('Long-hold threshold (seconds)'));
		o.datatype = 'range(3,9)'; o.default = '4';
		o.description = _('Keep below 10 seconds; the PMIC performs its own hard reset after a longer hold.');
		return m.render();
	}
});
