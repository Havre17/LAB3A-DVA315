#include <windows.h>
#include <stdio.h>
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
char newDisplayText[30];
char *curProcID;
BOOL readMessage, updatePlanet;
HWND newPlanetEdits[7];
HWND planetStatusHandles[3];
HWND hDlgs[2];
HWND curFoc, curInactiveFoc;
HANDLE clientMailslotHandle, mailslotC, mailslotS;
List *localPlanetList;
List *planetsToBeAdded;
LPCRITICAL_SECTION crit;
/*END ---- Globals*/

/*Planet status dialog helper functions - START*/

BOOL sendHelperFunc()
{
	int selItemCount = SendMessage(planetStatusHandles[1], LB_GETSELCOUNT, 0, 0);
	int *indexes = malloc(sizeof(int)*selItemCount);
	SendMessage(planetStatusHandles[1], LB_GETSELITEMS, selItemCount, indexes);
	char* nameHolder = NULL;
	int iLen = 0;
	planet_type* currentPlanetTmp1 = NULL;
	planet_type* currentPlanetTmp2 = NULL;
	for (int i = (selItemCount - 1); i >= 0; i--)
	{
		//Take the selected planets and send them one-by-one to the server for processing.
		currentPlanetTmp2 = (planet_type*)malloc(sizeof(planet_type));
		iLen = SendMessage(planetStatusHandles[1], LB_GETTEXTLEN, indexes[i], 0);
		nameHolder = malloc(sizeof(char)*iLen + 1);
		SendMessage(planetStatusHandles[1], LB_GETTEXT, indexes[i], nameHolder);
		currentPlanetTmp1 = GetPlanet(localPlanetList, curProcID, nameHolder);
		memcpy(currentPlanetTmp2, currentPlanetTmp1, sizeof(planet_type));
		mailslotWrite(mailslotS, currentPlanetTmp2, sizeof(planet_type));
		SendMessage(planetStatusHandles[0], LB_ADDSTRING, 0, nameHolder); //Add to sent planets list.
		SendMessage(planetStatusHandles[1], LB_DELETESTRING, indexes[i], 0); //Delete from local planets list.
		lpc--;
		free(currentPlanetTmp2);
		//Destroy_Item(localPlanetList, curProcID, nameHolder);
		free(nameHolder);
	}
	sprintf(newDisplayText, "Number of Local Planets: %d", lpc);
	SetWindowText(planetStatusHandles[2], newDisplayText);
	free(indexes);
	return TRUE;
}

BOOL loadFile()
{
	BOOL read = TRUE;
	char fileName[50];
	DWORD bytesRead;
	HANDLE fileHandle = OpenFileDialog("Solsystem", GENERIC_READ, OPEN_EXISTING);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	else
	{		
		if (localPlanetList == NULL)
		{
			localPlanetList = Create_List();
		}
		planet_type *pbuf = (planet_type*)malloc(sizeof(planet_type));
		while (read)
		{
			ReadFile(fileHandle, pbuf, sizeof(planet_type), &bytesRead, NULL);
			if (bytesRead > 0)
			{
				Add_Item_Last(localPlanetList, *pbuf);
				free(pbuf);
				pbuf = (planet_type*)malloc(sizeof(planet_type));
			}
			else
				read = FALSE;
			
		}
		return TRUE;
	}	
}

BOOL saveFile()
{
	HANDLE fileHandle = OpenFileDialog("Solsystem", GENERIC_WRITE, CREATE_NEW);
	if (fileHandle == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}
	
	else
	{	
		DWORD bytesWritten;
		int selCount = SendMessage(planetStatusHandles[1], LB_GETSELCOUNT, 0, 0), txtlen = 0;
		int *indexes = (int*)malloc(sizeof(int)*selCount);
		char* planetName = NULL;
		planet_type *temp = NULL;
		
		if (selCount > 1)
		{			
			SendMessage(planetStatusHandles[1], LB_GETSELITEMS, selCount, indexes);
		}
		else
		{
			SendMessage(planetStatusHandles[1], LB_SETSEL, TRUE, -1);
			SendMessage(planetStatusHandles[1], LB_GETSELITEMS, selCount, indexes);
		}
		for (int i = 0; i < selCount; i++)
		{
			planet_type *pbuf = (planet_type*)malloc(sizeof(planet_type));
			txtlen = SendMessage(newPlanetEdits[0], LB_GETTEXTLEN, indexes[i], NULL);
			planetName = (char*)malloc(sizeof(char)*txtlen);
			SendMessage(newPlanetEdits[0], LB_GETTEXT, indexes[i], planetName);
			temp = GetPlanet(localPlanetList, curProcID, planetName);
			memcpy(pbuf, temp, sizeof(planet_type));
			WriteFile(fileHandle, pbuf, sizeof(planet_type), &bytesWritten, NULL);
			free(pbuf);
		}
		return TRUE;
	}
}

/*Planet status dialog helper functions - END*/

/*New planet dialog helper functions - START*/

BOOL doneHelperFunc(HWND hDlg)
{
	char *txtHolder = NULL;
	planet_type *planetTmp1 = NULL, *planetTmp2 = NULL;
	int selCount = SendMessage(newPlanetEdits[0], LB_GETSELCOUNT, 0, 0), txtlen = 0, ret = 0;
	int *indexes = NULL;

	if (localPlanetList == NULL)
	{
		localPlanetList = Create_List();
	}
	/*All planets or selected ones.*/
	if (selCount < 1)
	{
		selCount = SendMessage(newPlanetEdits[0], LB_GETCOUNT, 0, 0);
		indexes = (int)malloc(sizeof(int)*selCount);
		ret = SendMessage(newPlanetEdits[0], LB_SETSEL, TRUE, -1);
		if (ret == LB_ERR)
		{
			MessageBox(hDlg, TEXT("LB_SETSEL return LB_ERR"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			return FALSE;
		}
		ret = SendMessage(newPlanetEdits[0], LB_GETSELITEMS, selCount, indexes);
		if (ret == LB_ERR)
		{
			MessageBox(hDlg, TEXT("LB_GETSELITEMS return LB_ERR"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			return FALSE;
		}
		memcpy(localPlanetList, planetsToBeAdded, (sizeof(planet_type)*planetsToBeAdded->Size)); free(indexes);
		for (int i = 1; i < 7; ++i)
		{
			SetWindowText(newPlanetEdits[i], "");
		}
		return TRUE;
	}
	else
	{
		indexes = (int)malloc(sizeof(int)*selCount);
		ret = SendMessage(newPlanetEdits[0], LB_GETSELITEMS, selCount, indexes);
		if (ret == LB_ERR)
		{
			MessageBox(hDlg, TEXT("LB_GETSELITEMS return LB_ERR"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			return FALSE;
		}
	}

	for (int i = (selCount - 1); i >= 0; i--)
	{
		//Take selected planets and add them to the local storage list.
		planetTmp2 = (planet_type*)malloc(sizeof(planet_type));
		txtlen = SendMessage(newPlanetEdits[0], LB_GETTEXTLEN, indexes[i], NULL);
		if (txtlen == LB_ERR)
		{
			MessageBox(hDlg, TEXT("Index parameter is invaild!"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
			free(indexes);
			return FALSE;
		}
		txtHolder = (char*)malloc(sizeof(char)*txtlen + 1);
		SendMessage(newPlanetEdits[0], LB_GETTEXT, indexes[i], txtHolder);
		/*planetTmp1 = GetPlanet(planetsToBeAdded, curProcID, txtHolder);	
		memcpy(planetTmp2, planetTmp1, sizeof(planet_type));
		Add_Item_Last(localPlanetList, *planetTmp2);*/
		
		SendMessage(newPlanetEdits[0], LB_DELETESTRING, indexes[i], 0);
		//Destroy_Item(planetsToBeAdded, curProcID, txtHolder);
		free(planetTmp2);
	}
	memcpy(localPlanetList, planetsToBeAdded, (sizeof(planet_type)*planetsToBeAdded->Size));
	free(indexes);
	for (int i = 1; i < 7; ++i)
	{
		SetWindowText(newPlanetEdits[i], "");
	}
	return TRUE;
}

BOOL deleteHelperFunc(HWND hDlg)
{
	int selCount = SendMessage(newPlanetEdits[0], LB_GETSELCOUNT, 0, 0);
	int *indexes = (int*)malloc(sizeof(int)*selCount);
	char* txtHolder;
	int txtlen = 0;
	SendMessage(newPlanetEdits[0], LB_GETSELITEMS, selCount, indexes);
	if (selCount < 1)
	{
		//If the user has not selected any planet, prompt question, if yes delete all planets
		if (MessageBox(hDlg, TEXT("Delete all the planets?"), TEXT("Close"),
			MB_ICONQUESTION | MB_YESNO) == IDYES)
		{
			Delete_List(planetsToBeAdded);
			SendMessage(newPlanetEdits[0], LB_RESETCONTENT, 0, 0);
			MessageBox(hDlg, TEXT("Items were deleted."), TEXT("Info"), (MB_ICONINFORMATION | MB_OK));
			free(indexes);
			return TRUE;
		}
	}
	else
	{
		for (int i = (selCount - 1); i >= 0; i--)
		{
			//Delete the selected items from the list.
			txtlen = SendMessage(newPlanetEdits[0], LB_GETTEXTLEN, indexes[i], (LPARAM)NULL);
			if (txtlen == LB_ERR)
			{
				MessageBox(hDlg, TEXT("Index parameter is invaild!"), TEXT("ERROR"), (MB_ICONERROR | MB_OK));
				free(indexes);
				return TRUE;
			}
			txtHolder = (char*)malloc(sizeof(char)*txtlen + 1);
			SendMessage(newPlanetEdits[0], LB_GETTEXT, indexes[i], txtHolder);
			Destroy_Item(planetsToBeAdded, curProcID, txtHolder);
			SendMessage(newPlanetEdits[0], LB_DELETESTRING, indexes[i], 0);
			free(txtHolder);
		}
		free(indexes);
		return TRUE;
	}
}

BOOL addHelperFunc(HWND hDlg)
{
	//Take info from all the editboxes and add them to a new planet item, then add to list.
	char *str = (char*)malloc(sizeof(char) * 20);
	planet_type *newPlanet = (planet_type*)malloc(sizeof(planet_type));
	GetWindowText(newPlanetEdits[1], str, 20);
	SendMessage(newPlanetEdits[0], LB_ADDSTRING, 0, (LPARAM)str);
	strcpy(newPlanet->name, str);
	GetWindowText(newPlanetEdits[2], str, 20);
	newPlanet->life = atoi(str);
	GetWindowText(newPlanetEdits[3], str, 20);
	newPlanet->mass = atof(str);
	GetWindowText(newPlanetEdits[4], str, 20);
	newPlanet->sx = atof(str);
	GetWindowText(newPlanetEdits[5], str, 20);
	newPlanet->sy = atof(str);
	GetWindowText(newPlanetEdits[6], str, 20);
	newPlanet->vx = atof(str);
	GetWindowText(newPlanetEdits[7], str, 20);
	newPlanet->vy = atof(str);
	strcpy(newPlanet->pid, curProcID);
	if (planetsToBeAdded == NULL)
	{
		planetsToBeAdded = Create_List();
	}
	Add_Item_Last(planetsToBeAdded, *newPlanet);
	free(newPlanet);
	free(str);
	for (int i = 1; i < 7; i++)
	{
		SetWindowText(newPlanetEdits[i], "");
	}
	return TRUE;
}

BOOL loadPlanetsHelperFunc()
{
	planet_type *iterator = localPlanetList->Head;
	char *pName = NULL;
	SendMessage(planetStatusHandles[1], LB_RESETCONTENT, 0, 0);
	lpc = 0;
	while (iterator != NULL)
	{
		pName = (char*)malloc(sizeof(char)*strlen(iterator->name));
		strcpy(pName, iterator->name);
		pName = iterator->name;
		SendMessage(planetStatusHandles[1], LB_ADDSTRING, 0, pName);
		iterator = iterator->next;
		lpc++;
	}
	sprintf(newDisplayText, "Number of Local Planets: %d", lpc);
	SetWindowText(planetStatusHandles[2], newDisplayText);
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
			planetStatusHandles[0] = GetDlgItem(hDlg, ALIVE_SENT_LB);
			planetStatusHandles[1] = GetDlgItem(hDlg, LOCAL_PLANETS_LB);
			planetStatusHandles[2] = GetDlgItem(hDlg, PLANET_COUNTER_EB);
			planetStatusHandles[3] = GetDlgItem(hDlg, SERV_MSG_LIST);
			SendMessage(planetStatusHandles[3], LB_ADDSTRING, 0, "Connecting to server...");
			mailslotS = mailslotConnect("mailbox");
			if (mailslotS != INVALID_HANDLE_VALUE)
			{
				SendMessage(planetStatusHandles[3], LB_ADDSTRING, 0, "Connection to server succeded.");
			}
			else
			{
				SendMessage(planetStatusHandles[3], LB_ADDSTRING, 0, "Connection to server failed...");
			}
			sprintf(newDisplayText, "Number of Local Planets: %d", lpc);
			SetWindowText(planetStatusHandles[2], newDisplayText);
			return TRUE;
			break;
		}		

		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
			case SEND_BUTTON:
			{
				if (sendHelperFunc())
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
				if (saveFile())
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
			int index = SendMessage(planetStatusHandles[0], LB_FINDSTRING, -1, wParam);
			SendMessage(planetStatusHandles[0], LB_DELETESTRING, index, 0);
			//SendMessage(planetStatusHandles[3], LB_ADDSTRING, 0, lparam);
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
					SendMessage(planetStatusHandles[3], LB_ADDSTRING, 0, "Failure reading from mailslot. Bytes read were lesser than 1.");
					return FALSE;
				}

				else
				{
					SendMessage(planetStatusHandles[3], LB_ADDSTRING, 0, msg.Message);
					EnterCriticalSection(&crit);
					readMessage = FALSE;
					LeaveCriticalSection(&crit);
					return TRUE;
				}
			}
			else
			{
				SendMessage(planetStatusHandles[3], LB_ADDSTRING, 0, "GetMailSlotInfo failed, returned zero.");
				return FALSE;
			}
			break;
		}
	
		case MY_LOAD_PLANETS:
		{
			if (loadPlanetsHelperFunc())
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
			if (MessageBox(hDlg, TEXT("Close the program?"), TEXT("Close"),
				MB_ICONQUESTION | MB_YESNO) == IDYES)
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
	}
	return FALSE;
}
/*New planet Dialog Window(the secondary dialog)*/
INT_PTR CALLBACK newPlanetDialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lparam)
{
	switch (uMsg)
	{
		case WM_INITDIALOG:
		{
			newPlanetEdits[0] = GetDlgItem(hDlg, CURSOL_LIST);
			newPlanetEdits[1] = GetDlgItem(hDlg, P_NAME_EDIT);
			newPlanetEdits[2] = GetDlgItem(hDlg, LIFE_EDIT);
			newPlanetEdits[3] = GetDlgItem(hDlg, MASS_EDIT);
			newPlanetEdits[4] = GetDlgItem(hDlg, POS_X_EDIT);
			newPlanetEdits[5] = GetDlgItem(hDlg, POS_Y_EDIT);
			newPlanetEdits[6] = GetDlgItem(hDlg, VEL_X_EDIT);
			newPlanetEdits[7] = GetDlgItem(hDlg, VEL_Y_EDIT);
			return TRUE;
			break;
		}
		case WM_COMMAND:
		{
			switch (wParam)
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
			if (loadPlanetsHelperFunc())
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
			break;
		}
	}
		return FALSE;
}
/*Function run on seperate thread, listning on client mailslot.*/
void mailslotListener(char* procId)
{
	DWORD msgCount = 0, result = 0;
	clientMailslotHandle = mailslotCreate(procId);
	while (TRUE)
	{
		result = GetMailslotInfo(clientMailslotHandle, NULL, NULL, &msgCount, NULL);
		if (result != 0)
		{
			if (msgCount > 0)
			{
				EnterCriticalSection(&crit);
				readMessage = TRUE;
				LeaveCriticalSection(&crit);
			}
		}
		else
		{
			SendMessage(planetStatusHandles[3], LB_ADDSTRING, 0, "GetMailSlotInfo failed when trying to get from clientMailSlot, returned zero.");
		}
	}
}
/*Win32 Entry Point*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow) {

	BOOL ret;
	MSG msg;
	InitializeCriticalSection(&crit);
	int len = (int)(log10(GetCurrentProcessId()) + 1);
	curProcID = (char*)malloc(sizeof(char)*len+1);
	mailslotC = mailslotConnect(curProcID);
	sprintf(curProcID, "%d", GetCurrentProcessId());
	HANDLE msgThread = threadCreate((LPTHREAD_START_ROUTINE)mailslotListener, curProcID);
	hDlgs[0] = CreateDialogParam(hInstance, MAKEINTRESOURCE(PLANET_STATUS_DIG), 0, planetStatusDialogProc, 0);
	hDlgs[1] = CreateDialogParam(hInstance, MAKEINTRESOURCE(NEW_PLANET_DIALOG), 0, newPlanetDialogProc, 0);
	ShowWindow(hDlgs[0], SW_SHOW);
	ShowWindow(hDlgs[1], SW_SHOW);
	Sleep(500);

	while ((ret = GetMessage(&msg, 0, 0, 0)) != 0)
	{
		if (ret == -1)
		{
			return -1;
		}

		if (readMessage)
		{
			msg.message = MY_MSG;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		for (int i = 0; i < 1; i++)
		{
			if (!IsDialogMessage(hDlgs[i], &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		
	}
	CloseHandle(msgThread);
	len = GetLastError();
	MessageBox(MB_APPLMODAL, len , NULL, MB_OK);
	return 0;
}