#include "stdafx.h"
#include "resource.h"
#include <helpers/atl-misc.h>

// These GUIDs identify the variables within our component's configuration file.
static const GUID guid_cfg_fs = { 0x1d3c5d34, 0xc9f3, 0x4c5a, { 0x94, 0x5e, 0x64, 0xd9, 0x36, 0xad, 0x4c, 0x5f } };
static const GUID guid_cfg_channel = { 0x52b24082, 0x6350, 0x482a, { 0x87, 0xf, 0x41, 0x47, 0x8b, 0x29, 0xdb, 0x6b } };

enum {
	default_cfg_fs = 16000,
	default_cfg_channel = 1,
};

cfg_uint cfg_fs(guid_cfg_fs, default_cfg_fs);
cfg_uint cfg_channel(guid_cfg_channel, default_cfg_channel);


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
		COMMAND_HANDLER_EX(IDC_BOGO1, EN_CHANGE, OnEditChange)
		COMMAND_HANDLER_EX(IDC_BOGO2, EN_CHANGE, OnEditChange)
	END_MSG_MAP()
private:
	BOOL OnInitDialog(CWindow, LPARAM);
	void OnEditChange(UINT, int, CWindow);
	bool HasChanged();
	void OnChanged();

	const preferences_page_callback::ptr m_callback;
};

BOOL CMyPreferences::OnInitDialog(CWindow, LPARAM) {
	SetDlgItemInt(IDC_BOGO1, cfg_fs, FALSE);
	SetDlgItemInt(IDC_BOGO2, cfg_channel, FALSE);
	return FALSE;
}

void CMyPreferences::OnEditChange(UINT, int, CWindow) {
	// not much to do here
	OnChanged();
}

t_uint32 CMyPreferences::get_state() {
	t_uint32 state = preferences_state::resettable;
	if (HasChanged()) state |= preferences_state::changed;
	return state;
}

void CMyPreferences::reset() {
	SetDlgItemInt(IDC_BOGO1, default_cfg_fs, FALSE);
	SetDlgItemInt(IDC_BOGO2, default_cfg_channel, FALSE);
	OnChanged();
}

void CMyPreferences::apply() {
	cfg_fs = GetDlgItemInt(IDC_BOGO1, NULL, FALSE);
	cfg_channel = GetDlgItemInt(IDC_BOGO2, NULL, FALSE);
	
	OnChanged(); //our dialog content has not changed but the flags have - our currently shown values now match the settings so the apply button can be disabled
}

bool CMyPreferences::HasChanged() {
	//returns whether our dialog content is different from the current configuration (whether the apply button should be enabled or not)
	return GetDlgItemInt(IDC_BOGO1, NULL, FALSE) != cfg_fs || GetDlgItemInt(IDC_BOGO2, NULL, FALSE) != cfg_channel;
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
