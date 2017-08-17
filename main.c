#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "resource.h"
#include "wrapper.h"
#include "List.h"
#include <math.h>

#define MY_LOAD_PLANETS (WM_APP + 1) 
#define MY_MSG (WM_APP + 3)
#define MY_UPDATE_LOCAL_PLANETS (WM_APP + 4)

/*
	Stuff todo: 
	1) Bug when reading the message sent, nothing is read at reading end.
	2) 
*/
/*START ---- Globals*/
int lpc = 0;
char counter_text[30];
char *cur_proc_id;
BOOL new_message, connected, dlg_hidden;
HWND h_dlgs[2];
HANDLE h_client_mailslot, h_mailslot_client, h_mailslot_server;
List *local_planet_list;
LPCRITICAL_SECTION crit;

/*END ---- Globals*/

void CreateNewPlanet(planet_type *new_planet, BOOL create_from_exist, char* planet_name, HWND hDlg)
{
	if (create_from_exist) {
		planet_type *planet_to_copy = GetPlanet(local_planet_list, cur_proc_id, planet_name);
		new_planet->life = planet_to_copy->life;
		new_planet->mass = planet_to_copy->mass;
		new_planet->next = planet_to_copy->next;
		new_planet->sx = planet_to_copy->sx;
		new_planet->sy = planet_to_copy->sy;
		new_planet->vx = planet_to_copy->vx;
		new_planet->vy = planet_to_copy->vy;
		strcpy(new_planet->name, planet_to_copy->name);
		strcpy(new_planet->pid, cur_proc_id);
	}
	else {
		char *string_buffer = malloc(128);
		GetDlgItemText(hDlg, P_NAME_EDIT, new_planet->name, sizeof(new_planet->name));
		GetDlgItemText(hDlg, MASS_EDIT, string_buffer, 128);
		new_planet->mass = atof(string_buffer);
		GetDlgItemText(hDlg, VEL_X_EDIT, string_buffer, 128);
		new_planet->vx = atof(string_buffer);
		GetDlgItemText(hDlg, VEL_Y_EDIT, string_buffer, 128);
		new_planet->vy = atof(string_buffer);
		GetDlgItemText(hDlg, POS_X_EDIT, string_buffer, 128);
		new_planet->sx = atof(string_buffer);
		GetDlgItemText(hDlg, POS_Y_EDIT, string_buffer, 128);
		new_planet->sy = atof(string_buffer);
		GetDlgItemText(hDlg, LIFE_EDIT, string_buffer, 128);
		new_planet->life = atoi(string_buffer);
		strcpy(new_planet->pid, cur_proc_id);
	}
	return;
}

/*Planet status dialog helper functions - START*/

BOOL SendPlanetToServer(HWND hDlg)
{
	if (!connected) {
		MessageBox(hDlg, TEXT("No current connection to server, cannot send planet/planets"), TEXT("Error"), (MB_ICONERROR | MB_OK));
		return FALSE;
	}
	int item_count = SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETSELCOUNT, 0, 0), ret;
	if (item_count < 1) {
		MessageBox(hDlg, TEXT("No planet was selected. None was sent."), TEXT("Error"), (MB_ICONERROR | MB_OK));
		return FALSE;
	}
	int *indexes = malloc(sizeof(int) * item_count);
	SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETSELITEMS, item_count, indexes);
	char *planet_name = malloc(20);
	planet_type *planet_to_send = malloc(sizeof(planet_type));
	for (int i = (item_count - 1); i >= 0; i--)
	{
		//Take the selected planets and send them one-by-one to the server for processing.
		SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETTEXT, indexes[i], planet_name);
		CreateNewPlanet(planet_to_send, TRUE, planet_name, NULL);
		mailslotWrite(h_mailslot_server, planet_to_send, sizeof(planet_type));
		SendDlgItemMessage(hDlg, ALIVE_SENT_LB, LB_ADDSTRING, 0, planet_name);
		SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_DELETESTRING, indexes[i], 0);
		local_planet_list = Destroy_Item(local_planet_list, cur_proc_id, planet_name);
	}
	free(indexes);
	free(planet_name);
	free(planet_to_send);
	return TRUE;
}

BOOL LoadFile()
{
	BOOL read = TRUE, success;
	DWORD bytes_read;
	HANDLE file_handle = OpenFileDialog(NULL, GENERIC_READ, OPEN_EXISTING);
	if (file_handle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}
	else {	
		//if no list exists
		if (local_planet_list == NULL) {
			local_planet_list = Create_List();
		}
		planet_type *planet_buffer = malloc(sizeof(planet_type));

		while (read) {

			//returns true if read success else returns false
			success = ReadFile(file_handle, planet_buffer, sizeof(planet_type), &bytes_read, NULL);

			if (bytes_read > 0 && success) {
				Add_Item_Last(&local_planet_list, *planet_buffer);
			}
			//else read has failed.
			else {
				read = FALSE;
			}
		}
		free(planet_buffer);
		return TRUE;
	}	
}

BOOL SaveFile(HWND hDlg)
{
	DWORD bytes_written = 0;
	int item_count = SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETSELCOUNT, 0, 0), txt_len = 0;
	int *indexes = malloc(sizeof(int) * item_count);
	char* planet_name = NULL;
	planet_type *temp = NULL, *planet_buffer = NULL;
	//alloc memory
	planet_buffer = malloc(sizeof(planet_type));
	planet_name = malloc(sizeof(char) * 20);

	HANDLE file_handle = OpenFileDialog(NULL, GENERIC_WRITE, CREATE_ALWAYS);
	if (file_handle == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	else {
		//if any item is selected in the list
		if (item_count > 1) {
			SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETSELITEMS, item_count, indexes);
		}
		else {
			SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_SETSEL, TRUE, -1);
			item_count = SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETCOUNT, 0, 0);
			SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETSELITEMS, item_count, indexes);
		}

		for (int i = 0; i < item_count; i++) {
			SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETTEXT, indexes[i], planet_name);
			memcpy(planet_buffer, GetPlanet(local_planet_list, cur_proc_id, planet_name), sizeof(planet_type));
			WriteFile(file_handle, planet_buffer, sizeof(planet_type), &bytes_written, NULL);
		}
		free(planet_name);
		free(planet_buffer);
		return TRUE;
	}
}

/*Planet status dialog helper functions - END*/

/*New planet dialog helper functions - START*/

//Function that is used for freeing resources. Takes an arbitary amount of arguments and frees them
void ResetNewPlanetEdits(HWND hDlg)
{
	SetWindowText(GetDlgItem(hDlg, P_NAME_EDIT), "");
	SetWindowText(GetDlgItem(hDlg, MASS_EDIT), "");
	SetWindowText(GetDlgItem(hDlg, POS_X_EDIT), "");
	SetWindowText(GetDlgItem(hDlg, POS_Y_EDIT), "");
	SetWindowText(GetDlgItem(hDlg, VEL_X_EDIT), "");
	SetWindowText(GetDlgItem(hDlg, VEL_Y_EDIT), "");
	SetWindowText(GetDlgItem(hDlg, LIFE_EDIT), "");
	return;
}

BOOL AddNewPlanetToLocal(HWND hDlg)
{
	char *string_buffer = malloc(sizeof(char) * 256);
	//Will refer to the size of said pointer, e.g. will be sizeof planet_type pointer.
	planet_type *new_planet = malloc(sizeof(*new_planet));
	if (local_planet_list == NULL) {
		local_planet_list = Create_List();
	}
	//Create the new planet
	CreateNewPlanet(new_planet, FALSE, NULL, hDlg);
	Add_Item_Last(&local_planet_list, *new_planet);
	ResetNewPlanetEdits(hDlg);
	free(string_buffer);
	free(new_planet);
	return TRUE;
}

BOOL LoadLocalPNames(HWND hDlg)
{
	planet_type *iterator = local_planet_list->Head;
	char *planet_name = NULL;
	planet_name = malloc(sizeof(char) * 20);
	SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_RESETCONTENT, 0, 0);
	lpc = 0;
	while (iterator != NULL)
	{
		strcpy(planet_name, iterator->name);
		SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_ADDSTRING, 0, planet_name);
		iterator = iterator->next;
		lpc++;
	}
	sprintf(counter_text, "Number of Local Planets: %d", lpc);
	SetWindowText(GetDlgItem(hDlg, PLANET_COUNTER_EB), counter_text);
	free(planet_name);
	return TRUE;
}

BOOL ReadMailslotMessage(HWND hDlg)
{
	DWORD next_msg_size, result, bytes_read;
	char* display_string = malloc(32);
	MsgStruct msg;
	result = GetMailslotInfo(h_client_mailslot, NULL, &next_msg_size, NULL, NULL);
	if (result != 0)
	{
		bytes_read = mailslotRead(h_client_mailslot, &msg, next_msg_size);
		EnterCriticalSection(&crit);
		new_message = FALSE;
		LeaveCriticalSection(&crit);
		if (bytes_read < 1)
		{
			SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "Failure reading from mailslot. Bytes read were lesser than 1.");
			return FALSE;
		}

		else
		{
			sprintf(display_string, "%s: %s", msg.PlanName, msg.Message);
			SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, display_string);
			int index = SendDlgItemMessage(hDlg, ALIVE_SENT_LB, LB_FINDSTRING, -1, msg.PlanName);
			SendDlgItemMessage(hDlg, ALIVE_SENT_LB, LB_DELETESTRING, index, 0);
			return TRUE;
		}
	}
	else
	{
		free(display_string);
		SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "GetMailSlotInfo failed, returned zero.");
		return FALSE;
	}
	return TRUE;
}
/*New planet dialog helper functions - END*/

/*Main Dialog Window*/
INT_PTR CALLBACK PlanetStatusDialogProcedure(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lparam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "Connecting to server...");
			h_mailslot_server = mailslotConnect("mailbox");
			if (h_mailslot_server != INVALID_HANDLE_VALUE)
			{
				SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "Connection to server succeded.");
				connected = TRUE;
			}
			else
			{
				SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "Connection to server failed.");
				connected = FALSE;
			}
			sprintf(counter_text, "Number of Local Planets: %d", lpc);
			SetWindowText(GetDlgItem(hDlg, PLANET_COUNTER_EB), counter_text);
			return TRUE;
			break;
		}		

		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case SEND_BUTTON:
				{
					if (SendPlanetToServer(hDlg))
					{
						SendMessage(h_dlgs[0], MY_LOAD_PLANETS, NULL, NULL);
						MessageBox(hDlg, TEXT("Planet/Planets were sent to server successfully."), TEXT("Info"), (MB_ICONINFORMATION | MB_OK));
					}
					else
					{
						MessageBox(hDlg, TEXT("Planet/Planets were sent to server failed."), TEXT("Info"), (MB_ICONINFORMATION | MB_OK));
					}
					return TRUE;
					break;
				}	

				case LOAD_BUTTON:
				{
					if (LoadFile())
					{
						MessageBox(hDlg, TEXT("Local planets was loaded from file"), TEXT("Info"), (MB_ICONINFORMATION | MB_OK));
						SendMessage(hDlg, MY_LOAD_PLANETS, 0, 0);
						return TRUE;
					}
					else
					{
						MessageBox(hDlg, TEXT("Invalid handle value was returned, Source: Load from file."), TEXT("ERROR"), (MB_ICONEXCLAMATION | MB_OK));
						return TRUE;
					}
					break;
				}

				case SAVE_BUTTON:
				{				
					if (!SaveFile(hDlg))
					{
						MessageBox(hDlg, TEXT("Invalid handle value was returned, Source: Save to file."), TEXT("ERROR"), (MB_ICONEXCLAMATION | MB_OK));
						return TRUE;
					}
					else
					{
						MessageBox(hDlg, TEXT("Local planets was saved to file"), TEXT("Info"), (MB_ICONINFORMATION | MB_OK));
						return TRUE;
					}
					break;
				}

				case ADD_BUTTON:
				{
					if (dlg_hidden) {
						ShowWindow(h_dlgs[1], SW_SHOW);
						dlg_hidden = FALSE;
					}
					else {
						ShowWindow(h_dlgs[1], SW_HIDE);
						dlg_hidden = TRUE;
					}
					break;
				}
			}
			break;
		}

		case MY_MSG:
		{
			if (ReadMailslotMessage(hDlg))
				return TRUE;
			else
				return FALSE;

			break;
		}
	
		case MY_LOAD_PLANETS:
		{
			if (LoadLocalPNames(hDlg))
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
			break;
		}
		
		case WM_CLOSE:
		{
			if (MessageBox(hDlg, TEXT("Close the program?"), TEXT("Close"),	MB_ICONQUESTION | MB_YESNO) == IDYES)
			{
				DestroyWindow(hDlg);
			}
			return TRUE;
			break;
		}
		
		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return TRUE;
			break;
		}

		default:
		{
			return FALSE;
			break;
		}
			
	}
}
/*New planet Dialog Window(the secondary dialog)*/
INT_PTR CALLBACK NewPlanetDialogProcedure(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lparam)
{
	switch (uMsg)
	{
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case ADD_BUTTON:
				{	
					if (AddNewPlanetToLocal(hDlg))
					{
						SendMessage(h_dlgs[0], MY_LOAD_PLANETS, NULL, NULL);					
						return TRUE;
					}
					else
					{
						MessageBox(hDlg, TEXT("Planet/Planets were not added to current solar system, adding failed"), TEXT("Info"), (MB_ICONEXCLAMATION | MB_OK));
						return TRUE;
					}
					break;
				}
			}
		}
		default:
		{
			return FALSE;
			break;
		}
	}
}
/*Function run on seperate thread, listning on client mailslot.*/
void ListenForMessage(char* proc_id)
{
	DWORD msg_count = 0, result = 0;
	h_client_mailslot = mailslotCreate(proc_id);
	while (TRUE)
	{
		result = GetMailslotInfo(h_client_mailslot, NULL, NULL, &msg_count, NULL);
		if (result != 0)
		{
			if (msg_count > 0)
			{
				EnterCriticalSection(&crit);
				new_message = TRUE;
				LeaveCriticalSection(&crit);
			}
		}
		else
		{
			SendDlgItemMessage(h_dlgs[0], SERV_MSG_LIST, LB_ADDSTRING, 0, "GetMailSlotInfo failed when trying to get from clientMailSlot, returned zero.");
		}
	}
}

/*Win32 Entry Point*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {

	BOOL ret;
	HANDLE message_thread;
	MSG msg;
	new_message = FALSE;
	InitializeCriticalSection(&crit);
	int len = (int)(log10(GetCurrentProcessId()) + 1);
	cur_proc_id = malloc(sizeof(char) * len);
	local_planet_list = Create_List();
	sprintf(cur_proc_id, "%d", GetCurrentProcessId());
	h_dlgs[0] = CreateDialog(hInstance, MAKEINTRESOURCE(PLANET_STATUS_DIG), 0, PlanetStatusDialogProcedure);
	h_dlgs[1] = CreateDialog(hInstance, MAKEINTRESOURCE(NEW_PLANET_DIALOG), 0, NewPlanetDialogProcedure);
	if (connected) {
		message_thread = threadCreate((LPTHREAD_START_ROUTINE)ListenForMessage, cur_proc_id);
		Sleep(500);
		h_mailslot_client = mailslotConnect(cur_proc_id);
		if (h_mailslot_client == INVALID_HANDLE_VALUE) {
			SendDlgItemMessage(h_dlgs[0], SERV_MSG_LIST, LB_ADDSTRING, 0, "Connection to client mailslot failed.");
		}
	}
	ShowWindow(h_dlgs[0], SW_SHOW);
	dlg_hidden = TRUE;
	ShowWindow(h_dlgs[1], SW_HIDE);

	while ((ret = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if (ret == -1)
		{
			return -1;
		}
		if (new_message)
		{
			msg.message = MY_MSG;
		}
		if (!IsDialogMessage(h_dlgs[0], &msg) && !IsDialogMessage(h_dlgs[1], &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	if (!CloseHandle(message_thread)) {
		MessageBox(MB_APPLMODAL, GetLastError(), "Error", MB_OK);
	}
	return 0;
}