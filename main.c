#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "resource.h"
#include "wrapper.h"
#include "List.h"
#include <math.h>

#define MY_LOAD_PLANETS (WM_APP + 1) 
#define MY_UPDATE_ALIVE (WM_APP + 2)
#define MY_MSG (WM_APP + 3)

/*
	Stuff todo: 
	1) Bug when reading the message sent, nothing is read at reading end.
	2) 
*/
/*START ---- Globals*/
int lpc = 0;
int cid = 0;
char counter_text[30];
char *cur_proc_id;
BOOL readMessage;
HWND hDlgs[2];
HWND curFoc, curInactiveFoc;
HANDLE clientMailslotHandle, mailslotC, mailslotS;
List *localPlanetList;
List *planetsToBeAdded;
LPCRITICAL_SECTION crit;
/*END ---- Globals*/

void CleanUp(int num, ...)
{
	va_list valist;

	va_start(valist, num);

	for (int i = 0; i < num; i++) {
		free(va_arg(valist, void*));
	}

	va_end(valist);
}

/*Planet status dialog helper functions - START*/

BOOL sendHelperFunc(HWND hDlg)
{
	int item_count = SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETSELCOUNT, 0, 0);
	int *indexes = malloc(sizeof(int) * item_count);
	SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETSELITEMS, item_count, indexes);
	char* planet_name = NULL;
	int name_len = 0;
	planet_type* cur_planet = NULL;
	for (int i = (item_count - 1); i >= 0; i--)
	{
		//Take the selected planets and send them one-by-one to the server for processing.
		planet_name = malloc(sizeof(char) * name_len);
		cur_planet = malloc(sizeof(planet_type*));
		name_len = SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETTEXTLEN, indexes[i], 0);
		SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETTEXT, indexes[i], planet_name);
		memcpy(GetPlanet(localPlanetList, cur_proc_id, planet_name), cur_planet, sizeof(planet_type));
		mailslotWrite(mailslotS, cur_planet, sizeof(planet_type));
		SendDlgItemMessage(hDlg, ALIVE_SENT_LB, LB_ADDSTRING, 0, planet_name);
		SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_DELETESTRING, indexes[i], 0);
		CleanUp(2, planet_name, cur_planet);
		lpc--;
	}
	sprintf(counter_text, "Number of Local Planets: %d", lpc);
	SetWindowText(GetDlgItem(hDlg, PLANET_COUNTER_EB), counter_text);
	CleanUp(1, indexes);
	return TRUE;
}

BOOL loadFile()
{
	BOOL read = TRUE, success;
	DWORD bytes_read;
	HANDLE file_handle = OpenFileDialog(NULL, GENERIC_READ, OPEN_EXISTING);
	if (file_handle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	else
	{	
		//if no list exists
		if (localPlanetList == NULL) {
			localPlanetList = Create_List();
		}
		//allocate for planets until there's no more. planets are read one at the time
		planet_type *planet_buffer = malloc(sizeof(planet_type*));

		while (read) {

			//returns true if read success else returns false
			success = ReadFile(file_handle, planet_buffer, sizeof(planet_type*), &bytes_read, NULL);

			if (bytes_read > 0 && success) {
				Add_Item_Last(localPlanetList, *planet_buffer);
				free(planet_buffer);
				planet_buffer = malloc(sizeof(planet_type*));
			}
			//else read has failed.
			else {
				read = FALSE;
			}

		}
		return TRUE;
	}	
}

BOOL saveFile(HWND hDlg)
{
	HANDLE file_handle = OpenFileDialog(NULL, GENERIC_WRITE, CREATE_NEW);
	DWORD bytesWritten = 0;
	int item_count = SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETSELCOUNT, 0, 0), txt_len = 0;
	int *indexes = malloc(sizeof(int*) * item_count);
	char* planet_name = NULL;
	planet_type *temp = NULL, *planet_buffer = NULL;

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
			planet_buffer = malloc(sizeof(planet_type*));
			txt_len = SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETTEXTLEN, indexes[i], NULL);
			planet_name = malloc(sizeof(char*) * txt_len);
			SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_GETTEXT, indexes[i], planet_name);
			memcpy(planet_buffer, GetPlanet(localPlanetList, cur_proc_id, planet_name), sizeof(planet_type*));
			WriteFile(file_handle, planet_buffer, sizeof(planet_type*), &bytesWritten, NULL);
			free(planet_buffer);
		}

		return TRUE;
	}
}

/*Planet status dialog helper functions - END*/

/*New planet dialog helper functions - START*/

//Copies each node from the list a to list b
void CopyList(List *a, List *b)
{
	planet_type *node_ptr_a = a->Head;
	
	while (node_ptr_a != NULL) {
		Add_Item_Last(b, *node_ptr_a);
		node_ptr_a = node_ptr_a->next;
	}
	return;
}

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

BOOL doneHelperFunc(HWND hDlg)
{
	char *string_buffer = NULL;
	planet_type *cur_planet = NULL;
	int item_count = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETSELCOUNT, 0, 0), string_len = 0, ret = 0;
	int *indexes = NULL;
	if (localPlanetList == NULL) {
		localPlanetList = Create_List();
	}
	/*All planets or selected ones.*/
	if (item_count < 1)
	{
		//get number of items in listbox
		item_count = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETCOUNT, 0, 0);
		indexes = malloc(sizeof(int*) * item_count);
		//select all the items in the list
		ret = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_SETSEL, TRUE, -1);

		if (ret == LB_ERR)
		{
			MessageBox(hDlg, TEXT("LB_SETSEL return LB_ERR"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			return FALSE;
		}

		ret = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETSELITEMS, item_count, indexes);

		if (ret == LB_ERR)
		{
			MessageBox(hDlg, TEXT("LB_GETSELITEMS return LB_ERR"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			return FALSE;
		}
		CopyList(planetsToBeAdded, localPlanetList);
		ResetNewPlanetEdits(hDlg);
		return TRUE;
	}
	else {
		indexes = malloc(sizeof(int*)*item_count);

		ret = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETSELITEMS, item_count, indexes);

		if (ret == LB_ERR) {
			MessageBox(hDlg, TEXT("LB_GETSELITEMS return LB_ERR"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			return FALSE;
		}
	}
	//work from down upwards due to dynamic index alteration
	for (int i = (item_count - 1); i >= 0; i--)
	{
		//Take selected planets and add them to the local storage list.
		string_len = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETTEXTLEN, indexes[i], NULL);
		if (string_len == LB_ERR) {
			MessageBox(hDlg, TEXT("Index parameter is invaild!"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			free(indexes);
			return FALSE;
		}

		string_buffer = malloc(sizeof(char*) * string_len + 1);
		ret = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETTEXT, indexes[i], string_buffer);

		if (ret == LB_ERR) {
			MessageBox(hDlg, TEXT("Error when getting text from lb."), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			free(indexes);
			return FALSE;
		}
		else {
			cur_planet = GetPlanet(planetsToBeAdded, cur_proc_id, string_buffer);
			Add_Item_Last(localPlanetList, *cur_planet);
			SendDlgItemMessage(hDlg, CURSOL_LIST, LB_DELETESTRING, indexes[i], NULL);
			Destroy_Item(planetsToBeAdded, cur_proc_id, string_buffer);
		}	
	}
	ResetNewPlanetEdits(hDlg);
	CleanUp(3, string_buffer, cur_planet, indexes);
	return TRUE;
}

BOOL deleteHelperFunc(HWND hDlg)
{
	int item_count = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETSELCOUNT, 0, 0);
	int *indexes = malloc(sizeof(int*) * item_count);
	char* string_buffer = NULL;
	int string_len = 0;
	if (item_count < 1)
	{
		//If the user has not selected any planet, prompt question, if yes delete all planets
		if (MessageBox(hDlg, TEXT("Delete all the planets?"), TEXT("Close"), MB_ICONQUESTION | MB_YESNO) == IDYES) {
			Delete_List(planetsToBeAdded);
			SendDlgItemMessage(hDlg, CURSOL_LIST, LB_RESETCONTENT, 0, 0);
			MessageBox(hDlg, TEXT("Items were deleted."), TEXT("Info"), (MB_ICONINFORMATION | MB_OK));
			free(indexes);
			return TRUE;
		}
	}
	else
	{
		for (int i = (item_count - 1); i >= 0; i--)
		{
			//Delete the selected items from the list.
			string_len = SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETTEXTLEN, indexes[i], NULL);
			if (string_len == LB_ERR)
			{
				MessageBox(hDlg, TEXT("Index parameter is invaild!"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
				free(indexes);
				return TRUE;
			}
			string_buffer = malloc(sizeof(char*)*string_len + 1);
			SendDlgItemMessage(hDlg, CURSOL_LIST, LB_GETTEXT, indexes[i], string_buffer);
			Destroy_Item(planetsToBeAdded, cur_proc_id, string_buffer);
			SendDlgItemMessage(hDlg, CURSOL_LIST, LB_DELETESTRING, indexes[i], 0);
			free(string_buffer);
		}
		free(indexes);
		return TRUE;
	}
}

BOOL addHelperFunc(HWND hDlg)
{
	//Take info from all the editboxes and add them to a new planet item, then add to list.
	char *str = malloc(sizeof(char*) * 20);
	planet_type *new_planet = malloc(sizeof(planet_type*));
	GetWindowText(GetDlgItem(hDlg, P_NAME_EDIT), str, 20);
	SendDlgItemMessage(hDlg, CURSOL_LIST, LB_ADDSTRING, 0, str);
	strcpy(new_planet->name, str);
	GetWindowText(GetDlgItem(hDlg, MASS_EDIT), str, 20);
	new_planet->life = atoi(str);
	GetWindowText(GetDlgItem(hDlg, POS_X_EDIT), str, 20);
	new_planet->mass = atof(str);
	GetWindowText(GetDlgItem(hDlg, POS_Y_EDIT), str, 20);
	new_planet->sx = atof(str);
	GetWindowText(GetDlgItem(hDlg, VEL_X_EDIT), str, 20);
	new_planet->sy = atof(str);
	GetWindowText(GetDlgItem(hDlg, VEL_Y_EDIT), str, 20);
	new_planet->vx = atof(str);
	GetWindowText(GetDlgItem(hDlg, LIFE_EDIT), str, 20);
	new_planet->vy = atof(str);
	strcpy(new_planet->pid, cur_proc_id);
	if (planetsToBeAdded == NULL)
	{
		planetsToBeAdded = Create_List();
	}
	Add_Item_Last(planetsToBeAdded, *new_planet);
	CleanUp(2, new_planet, str);
	ResetNewPlanetEdits(hDlg);
	return TRUE;
}

BOOL loadPlanetsHelperFunc(HWND hDlg)
{
	planet_type *iterator = localPlanetList->Head;
	char *planet_name = NULL;
	SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_RESETCONTENT, 0, 0);
	lpc = 0;
	while (iterator != NULL)
	{
		planet_name = malloc(sizeof(char*) * strlen(iterator->name));
		strcpy(planet_name, iterator->name);
		planet_name = iterator->name;
		SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_ADDSTRING, 0, planet_name);
		iterator = iterator->next;
		lpc++;
	}
	sprintf(counter_text, "Number of Local Planets: %d", lpc);
	SetWindowText(GetDlgItem(hDlg, PLANET_COUNTER_EB), counter_text);
	return TRUE;
}
/*New planet dialog helper functions - END*/

/*Main Dialog Window*/
INT_PTR CALLBACK planetStatusDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lparam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "Connecting to server...");
			mailslotS = mailslotConnect("mailbox");
			if (mailslotS != INVALID_HANDLE_VALUE)
			{
				SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "Connection to server succeded.");
			}
			else
			{
				SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "Connection to server failed.");
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
				if (sendHelperFunc(hDlg))
				{
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
				if (loadFile())
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
				if (saveFile(hDlg))
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
			}		
			break;
		}

		case MY_UPDATE_ALIVE:
		{
			int index = SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_FINDSTRING, -1, wParam);
			SendDlgItemMessage(hDlg, LOCAL_PLANETS_LB, LB_DELETESTRING, index, 0);;
			return TRUE;
			break;
		}

		case MY_MSG:
		{
			DWORD nextMsgSize, result;
			MsgStruct msg;
			result = GetMailslotInfo(mailslotC, NULL, &nextMsgSize, NULL, NULL);
			if (result != 0)
			{
				result = mailslotRead(mailslotC, &msg, nextMsgSize);
				if (result > 1)
				{
					SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "Failure reading from mailslot. Bytes read were lesser than 1.");
					return FALSE;
				}

				else
				{
					SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, msg.Message);
					EnterCriticalSection(&crit);
					readMessage = FALSE;
					LeaveCriticalSection(&crit);
					return TRUE;
				}
			}
			else
			{
				SendDlgItemMessage(hDlg, SERV_MSG_LIST, LB_ADDSTRING, 0, "GetMailSlotInfo failed, returned zero.");
				return FALSE;
			}
			break;
		}
	
		case MY_LOAD_PLANETS:
		{
			if (loadPlanetsHelperFunc(hDlg))
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
INT_PTR CALLBACK newPlanetDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lparam)
{
	switch (uMsg)
	{
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case DONE_BUTTON:
				{
					if (doneHelperFunc(hDlg))
					{
						MessageBox(hDlg, TEXT("Planet/Planets were added to local storage."), TEXT("Info"), (MB_ICONINFORMATION | MB_OK));
						SendMessage(hDlg, MY_LOAD_PLANETS, 0, 0);
						return TRUE;
					}
					else
					{
						MessageBox(hDlg, TEXT("Planet/Planets were not added to local storage, adding failed"), TEXT("Info"), (MB_ICONEXCLAMATION | MB_OK));
						return FALSE;
					}	
					break;
				}
		
				case DELETE_BUTTON:
				{
					if (deleteHelperFunc(hDlg))
					{
						MessageBox(hDlg, TEXT("Planet/Planets were deleted successfully."), TEXT("Info"), (MB_ICONINFORMATION | MB_OK));
						return TRUE;
					}
					else
					{
						MessageBox(hDlg, TEXT("Planet/Planets were not deleted, deleting failed."), TEXT("Info"), (MB_ICONEXCLAMATION | MB_OK));
						return FALSE;
					}
					break;
				}
		
				case ADD_BUTTON:
				{	
					if (addHelperFunc(hDlg))
					{
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
			break;
		}
		case MY_LOAD_PLANETS:
		{
			if (loadPlanetsHelperFunc(hDlg))
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
			break;
		}

		default:
		{
			return FALSE;
			break;
		}
	}
}
/*Function run on seperate thread, listning on client mailslot.*/
void mailslotListener(char* proc_id)
{
	DWORD msg_count = 0, result = 0;
	clientMailslotHandle = mailslotCreate(proc_id);
	while (TRUE)
	{
		result = GetMailslotInfo(clientMailslotHandle, NULL, NULL, &msg_count, NULL);
		if (result != 0)
		{
			if (msg_count > 0)
			{
				EnterCriticalSection(&crit);
				readMessage = TRUE;
				LeaveCriticalSection(&crit);
			}
		}
		else
		{
			SendDlgItemMessage(hDlgs[0], SERV_MSG_LIST, LB_ADDSTRING, 0, "GetMailSlotInfo failed when trying to get from clientMailSlot, returned zero.");
		}
	}
}

/*Win32 Entry Point*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {

	BOOL ret;
	MSG msg;
	InitializeCriticalSection(&crit);
	int len = (int)(log10(GetCurrentProcessId()) + 1);
	cur_proc_id = malloc(sizeof(char*) * len);
	localPlanetList = Create_List();
	sprintf(cur_proc_id, "%d", GetCurrentProcessId());
	mailslotC = mailslotConnect(cur_proc_id);
	HANDLE message_thread = threadCreate((LPTHREAD_START_ROUTINE)mailslotListener, cur_proc_id);
	hDlgs[0] = CreateDialogParam(hInstance, MAKEINTRESOURCE(PLANET_STATUS_DIG), 0, planetStatusDialogProc, 0);
	hDlgs[1] = CreateDialogParam(hInstance, MAKEINTRESOURCE(NEW_PLANET_DIALOG), 0, newPlanetDialogProc, 0);
	ShowWindow(hDlgs[0], SW_SHOW);
	ShowWindow(hDlgs[1], SW_SHOW);
	Sleep(500);

	while ((ret = GetMessage(&msg, 0, 0, 0)) > 0)
	{
		if (ret == -1)
		{
			return -1;
		}

		//if (readMessage)
		//{
		//	msg.message = MY_MSG;
		//	TranslateMessage(&msg);
		//	DispatchMessage(&msg);
		//}
		for (int i = 0; i < 1; i++)
		{
			if (!IsDialogMessage(hDlgs[i], &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}	
	}
	if (!CloseHandle(message_thread)) {
		MessageBox(MB_APPLMODAL, GetLastError(), "Error", MB_OK);
	}
	return 0;
}