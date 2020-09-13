#include "stdafx.h"
#include "resource.h"
#include <helpers/atl-misc.h>
#include <helpers/dropdown_helper.h>

// These GUIDs identify the variables within our component's configuration file.
static const GUID guid_cfg_fs = { 0x1d3c5d34, 0xc9f3, 0x4c5a, { 0x94, 0x5e, 0x64, 0xd9, 0x36, 0xad, 0x4c, 0x5f } };
static const GUID guid_cfg_channel = { 0x52b24082, 0x6350, 0x482a, { 0x87, 0xf, 0x41, 0x47, 0x8b, 0x29, 0xdb, 0x6b } };
static const GUID guid_cfg_history_rate = { 0x5bcb64d3, 0x9f1c, 0x4045, { 0xb6, 0x9e, 0xf5, 0x9, 0x56, 0x6f, 0x37, 0xed } };
static const GUID guid_cfg_bits = { 0x77d6f65a, 0xda3c, 0x4224, { 0x9a, 0xe2, 0x3d, 0x92, 0x79, 0x33, 0x93, 0xfe } };

enum {
	default_cfg_fs = 16000,
	default_cfg_channel = 1,
	default_cfg_bits = 16,
};

cfg_uint cfg_fs(guid_cfg_fs, default_cfg_fs);
cfg_uint cfg_channel(guid_cfg_channel, default_cfg_channel);
cfg_int cfg_bits(guid_cfg_bits, default_cfg_bits);
static cfg_dropdown_history cfg_history_rate(guid_cfg_history_rate, 32);
static const unsigned srate_tab[] = { 8000, 11025, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 96000, 176400, 192000, 352800, 384000 };
static const int bits_tab[] = { 8, 16, 24, 32, -32 };
static int bits_to_idx(int bits) {
	for (int i = 0; i < sizeof(bits_tab) / sizeof(bits_tab[0]); i++)
		if (bits == bits_tab[i])
			return i;
	return -1;
}


class CMyPreferences : public CDialogImpl<CMyPreferences>, public preferences_page_instance {
public:
	//Constructor - invoked by preferences_page_impl helpers - don't do Create() in here, preferences_page_impl does this for us
	CMyPreferences(preferences_page_callback::ptr callback) : m_callback(callback) {}

	//Note that we don't bother doing anything regarding destruction of our class.
	//The host ensures that our dialog is destroyed first, then the last reference to our preferences_page_instance object is released, causing our object to be deleted.


	//dialog resource ID
	enum {IDD = IDD_MYPREFERENCES};
	// preferences_page_instance methods (not all of them - get_wnd() is supplied by preferences_page_impl helpers)
	t_uint32 get_state();
	void apply();
	void reset();

	//WTL message map
	BEGIN_MSG_MAP_EX(CMyPreferences)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_FS, CBN_EDITCHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_FS, CBN_SELCHANGE, OnSelectionChange)
		DROPDOWN_HISTORY_HANDLER(IDC_FS, cfg_history_rate)
		COMMAND_HANDLER_EX(IDC_CHANNEL, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_ENCODING, CBN_SELCHANGE, OnSelectionChange)
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM);
	void OnEditChange(UINT, int, CWindow);
	void OnSelectionChange(UINT, int, CWindow);
	bool HasChanged();
	void OnChanged();

	const preferences_page_callback::ptr m_callback;
};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM) {
	char temp[16] = {};
	for (int n = tabsize(srate_tab); n--;) {
		if (srate_tab[n] == cfg_fs) continue;

		snprintf(temp, sizeof(temp), "%u", srate_tab[n]);
		cfg_history_rate.add_item(temp);
	}
	snprintf(temp, sizeof(temp), "%u", cfg_fs.get_value());
	cfg_history_rate.add_item(temp);
	CWindow w = GetDlgItem(IDC_FS);
	cfg_history_rate.setup_dropdown(w);
	::SendMessage(w, CB_SETCURSEL, 0, 0);

	SetDlgItemInt(IDC_CHANNEL, cfg_channel, FALSE);

	w = GetDlgItem(IDC_ENCODING);
	uSendMessageText(w, CB_ADDSTRING, 0, "Unsigned 8-bit int");
	uSendMessageText(w, CB_ADDSTRING, 0, "Signed 16-bit int");
	uSendMessageText(w, CB_ADDSTRING, 0, "Signed 24-bit int");
	uSendMessageText(w, CB_ADDSTRING, 0, "Signed 32-bit int");
	uSendMessageText(w, CB_ADDSTRING, 0, "32-bit float");
	::SendMessage(w, CB_SETCURSEL, bits_to_idx(cfg_bits), 0);

	return FALSE;
}

void CMyPreferences::OnEditChange(UINT, int, CWindow) {
	OnChanged();
}

void CMyPreferences::OnSelectionChange(UINT, int, CWindow) {
	OnChanged();
}

t_uint32 CMyPreferences::get_state() {
	t_uint32 state = preferences_state::resettable;
	if (HasChanged()) state |= preferences_state::changed;
	return state;
}

void CMyPreferences::reset() {
	SetDlgItemInt(IDC_FS, default_cfg_fs, FALSE);
	SetDlgItemInt(IDC_CHANNEL, default_cfg_channel, FALSE);
	SendDlgItemMessage(IDC_ENCODING, CB_SETCURSEL, bits_to_idx(default_cfg_bits));

	OnChanged();
}

void CMyPreferences::apply() {
	char temp[16];
	unsigned fs = GetDlgItemInt(IDC_FS, NULL, FALSE);
	if (fs < 1000) fs = 1000;
	SetDlgItemInt(IDC_FS, fs, FALSE);
	snprintf(temp, sizeof(temp), "%u", fs);
	cfg_history_rate.add_item(temp);
	cfg_fs = fs;

	unsigned channel = GetDlgItemInt(IDC_CHANNEL, NULL, FALSE);
	if (channel < 1) channel = 1;
	if (8 < channel) channel = 8;
	SetDlgItemInt(IDC_CHANNEL, channel, FALSE);
	cfg_channel = channel;

	cfg_bits = bits_tab[SendDlgItemMessage(IDC_ENCODING, CB_GETCURSEL)];

	OnChanged(); //our dialog content has not changed but the flags have - our currently shown values now match the settings so the apply button can be disabled
}

bool CMyPreferences::HasChanged() {
	//returns whether our dialog content is different from the current configuration (whether the apply button should be enabled or not)
	if (GetDlgItemInt(IDC_FS, NULL, FALSE) != cfg_fs) return true;
	if (GetDlgItemInt(IDC_CHANNEL, NULL, FALSE) != cfg_channel) return true;
	if (SendDlgItemMessage(IDC_ENCODING, CB_GETCURSEL) != bits_to_idx(cfg_bits)) return true;

	return false;
}
void CMyPreferences::OnChanged() {
	//tell the host that our state has changed to enable/disable the apply button appropriately.
	m_callback->on_state_changed();
}

class preferences_page_myimpl : public preferences_page_impl<CMyPreferences> {
	// preferences_page_impl<> helper deals with instantiation of our dialog; inherits from preferences_page_v3.
public:
	const char * get_name() {return "Raw PCM Player";}
	GUID get_guid() {
		static const GUID guid = { 0xb3232742, 0x3dee, 0x491b, { 0xa9, 0x34, 0x7c, 0x9e, 0x1c, 0x4c, 0x31, 0x99 } };
		return guid;
	}
	GUID get_parent_guid() {return guid_input;}
};

static preferences_page_factory_t<preferences_page_myimpl> g_preferences_page_myimpl_factory;
