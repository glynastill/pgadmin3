//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
// RCS-ID:      $Id$
// Copyright (C) 2002 - 2003, The pgAdmin Development Team
// This software is released under the Artistic Licence
//
// dlgQuery.cpp - Property Dialog
//
//////////////////////////////////////////////////////////////////////////

// wxWindows headers
#include <wx/wx.h>
#include <wx/button.h>

// App headers
#include "pgAdmin3.h"
#include "ctlSQLBox.h"
#include "pgCollection.h"
#include "pgDatatype.h"
#include "misc.h"
#include "menu.h"
#include "pgDefs.h"
#include <wx/imaglist.h>

// Images
#include "images/properties.xpm"

#include "frmMain.h"
#include "frmHelp.h"

// Property dialogs
#include "dlgProperty.h"
#include "dlgUser.h"
#include "dlgGroup.h"
#include "dlgDatabase.h"
#include "dlgCast.h"
#include "dlgLanguage.h"
#include "dlgSchema.h"
#include "dlgAggregate.h"
#include "dlgConversion.h"
#include "dlgDomain.h"
#include "dlgFunction.h"
#include "dlgOperator.h"
#include "dlgSequence.h"
#include "dlgTable.h"
#include "dlgColumn.h"
#include "dlgIndex.h"
#include "dlgIndexConstraint.h"
#include "dlgForeignKey.h"
#include "dlgCheck.h"
#include "dlgRule.h"
#include "dlgTrigger.h"
#include "dlgType.h"
#include "dlgView.h"
#include "dlgJob.h"
#include "dlgStep.h"
#include "dlgSchedule.h"

#include "pgTable.h"
#include "pgColumn.h"
#include "pgTrigger.h"

enum
{
    CTL_PROPSQL=250,
    CTL_LBPRIV,
    CTL_STATICGROUP,
    CTL_CBGROUP,
    CTL_ADDPRIV,
    CTL_DELPRIV,
    CTL_ALLPRIV,
    CTL_ALLPRIVGRANT,
    CTL_PRIVCB          // base for all privilege checkboxes, must be last
};


BEGIN_EVENT_TABLE(dlgProperty, DialogWithHelp)
    EVT_NOTEBOOK_PAGE_CHANGED(XRCID("nbNotebook"),  dlgProperty::OnPageSelect)  
    EVT_BUTTON (XRCID("btnHelp"),                   dlgProperty::OnHelp)
    EVT_BUTTON (XRCID("btnOK"),                     dlgProperty::OnOK)
    EVT_BUTTON (XRCID("btnCancel"),                 dlgProperty::OnCancel)
    EVT_CLOSE(                                      dlgProperty::OnClose)
END_EVENT_TABLE();


dlgProperty::dlgProperty(frmMain *frame, const wxString &resName) : DialogWithHelp(frame)
{
    readOnly=false;
    objectType=-1;
    sqlPane=0;
    processing=false;
    mainForm=frame;
    wxWindowBase::SetFont(settings->GetSystemFont());
    wxXmlResource::Get()->LoadDialog(this, frame, resName);
    nbNotebook = CTRL_NOTEBOOK("nbNotebook");

    if (!nbNotebook)
    {
        wxMessageBox(wxString::Format(_("Problem with resource %s: Notebook not found.\nPrepare to crash!"), resName.c_str()));
        return;
    }
    SetIcon(wxIcon(properties_xpm));

#ifdef __WIN32__
    wxNotebookPage *page=nbNotebook->GetPage(0);
    wxASSERT(page != NULL);
    page->GetClientSize(&width, &height);
#else
    nbNotebook->GetClientSize(&width, &height);
	height -= 35;   // sizes of tabs
#endif

    numericValidator.SetStyle(wxFILTER_NUMERIC);
    btnOK->Disable();

    wxSize size=GetSize();

    if (wxWindow::FindWindow(XRCID("txtStatus")))
        statusBox=CTRL_TEXT("txtStatus");
    else
    {
        wxSize stdTxtSize=ConvertDialogToPixels(wxSize(0, 12));
        size.SetHeight(size.GetHeight()+stdTxtSize.GetHeight());
        SetSize(size);
        size=GetClientSize();
        wxPoint pos(0, size.GetHeight()-stdTxtSize.GetHeight());
        size.SetHeight(stdTxtSize.GetHeight());
        statusBox = new wxTextCtrl(this, 178, wxT(""), pos, size, wxTE_READONLY);
    }

    statusBox->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
}


dlgProperty::~dlgProperty()
{
    wxString prop = wxT("Properties/") + wxString(typesList[objectType].typName);
    settings->Write(prop, GetPosition());

    if (GetWindowStyle() & wxTHICK_FRAME)
        settings->Write(prop, GetSize());
}


wxString dlgProperty::GetHelpPage() const
{
    wxString page;

    pgObject *obj=0; //GetObject();
    if (obj)
        page=obj->GetHelpPage(false);
    else
    {
        page=wxT("sql-create") + wxString(typesList[objectType].typName).Lower();
    }
    return page;
}


void dlgProperty::CheckValid(bool &enable, const bool condition, const wxString &msg)
{
    if (enable)
    {
        if (!condition)
        {
            statusBox->SetValue(msg);
            enable=false;
        }
    }
}


void dlgProperty::EnableOK(bool enable)
{
    btnOK->Enable(enable);
    if (enable)
        statusBox->SetValue(wxT(""));
}


int dlgProperty::Go(bool modal)
{
    // restore previous position and size, if applicable
    wxString prop = wxT("Properties/") + wxString(typesList[objectType].typName);

    if (GetWindowStyle() & wxTHICK_FRAME)
        SetSize(settings->Read(prop, GetSize()));

    wxPoint pos=settings->Read(prop, GetPosition());
    if (pos.x >= 0 && pos.y >= 0)
        Move(pos);

    if (GetObject())
    {
        if (!readOnly && !GetObject()->CanCreate())
        {
            // users who can't create will usually not be allowed to change either.
            readOnly=false;
        }

        SetTitle(wxString(typesList[objectType].typName) + wxT(" ") + GetObject()->GetFullIdentifier());
    }
    else
        SetTitle(wxGetTranslation(typesList[objectType].newString));

    if (modal)
        return ShowModal();
    else
        Show(true);
    return 0;
}

void dlgProperty::CreateAdditionalPages()
{
    sqlPane = new ctlSQLBox(nbNotebook, CTL_PROPSQL, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxSUNKEN_BORDER | wxTE_READONLY | wxTE_RICH2);
    nbNotebook->AddPage(sqlPane, wxT("SQL"));
}


wxString dlgProperty::GetName()
{
    return txtName->GetValue().Strip(wxString::both);
}


void dlgProperty::AppendComment(wxString &sql, const wxString &objName, pgObject *obj)
{
    wxString comment=txtComment->GetValue();
    if ((!obj && !comment.IsEmpty()) ||(obj && obj->GetComment() != comment))
    {
        sql += wxT("COMMENT ON ") + objName
            + wxT(" IS ") + qtString(comment) + wxT(";\n");
    }
}


void dlgProperty::AppendComment(wxString &sql, const wxString &objType, pgSchema *schema, pgObject *obj)
{
    wxString comment=txtComment->GetValue();
    if ((!obj && !comment.IsEmpty()) ||(obj && obj->GetComment() != comment))
    {
        sql += wxT("COMMENT ON ") + objType + wxT(" ");
        if (schema)
           sql += schema->GetQuotedIdentifier() + wxT(".");
        sql += qtIdent(GetName()) + wxT(" IS ") + qtString(comment) + wxT(";\n");
    }
}


void dlgProperty::AppendQuoted(wxString &sql, const wxString &name)
{
    if (name.First('.') >= 0)
    {
        sql += qtIdent(name.BeforeFirst('.')) + wxT(".") + qtIdent(name.AfterFirst('.'));
    }
    else
        sql += qtIdent(name);
}


void dlgProperty::OnClose(wxCloseEvent &ev)
{
    if (IsModal())
        EndModal(-1);
    else
        Destroy();
}



bool dlgProperty::tryUpdate(wxTreeItemId collectionItem)
{
    wxTreeCtrl *browser=mainForm->GetBrowser();
    pgCollection *collection = (pgCollection*)browser->GetItemData(collectionItem);
    if (collection && collection->IsCollection() && collection->IsCollectionForType(objectType))
    {
        pgObject *data = CreateObject(collection);
        if (data)
        {
            wxString nodeName=data->GetFullName();
            size_t pos=0;
            wxTreeItemId item;

            if (data->GetType() != PG_COLUMN)
            {
                // columns should be appended, not inserted alphabetically

                wxCookieType cookie;
                item=browser->GetFirstChild(collectionItem, cookie);
                while (item)
                {
                    if (browser->GetItemText(item) > nodeName)
                        break;
                    pos++;
                    item=browser->GetNextChild(collectionItem, cookie);
                }
            }

            if (item)
                browser->InsertItem(collectionItem, pos, nodeName, data->GetIcon(), -1, data);
            else    
                browser->AppendItem(collectionItem, nodeName, data->GetIcon(), -1, data);

            if (data->WantDummyChild())
                browser->AppendItem(data->GetId(), wxT("Dummy"));

            if (browser->GetSelection() == collectionItem)
                collection->ShowTreeDetail(browser, 0, mainForm->GetProperties());
            else
                collection->UpdateChildCount(browser);
        }
        return true;
    }
    return false;
}



void dlgProperty::ShowObject()
{
    pgObject *data=GetObject();
    if (data)
    {
        pgObject *newData=data->Refresh(mainForm->GetBrowser(), item);
        if (newData && newData != data)
        {
            mainForm->GetBrowser()->SetItemData(item, newData);
            mainForm->GetBrowser()->SetItemImage(item, newData->GetIcon(), wxTreeItemIcon_Normal);
            mainForm->GetBrowser()->SetItemImage(item, newData->GetIcon(), wxTreeItemIcon_Selected);
            newData->SetId(item);
            delete data;
        }
        if (newData)
        {
            mainForm->GetBrowser()->DeleteChildren(newData->GetId());

            newData->ShowTree(mainForm, mainForm->GetBrowser(), mainForm->GetProperties(), 0);
            mainForm->GetBrowser()->SetItemText(item, newData->GetFullName());
            mainForm->GetSqlPane()->SetReadOnly(false);
            mainForm->GetSqlPane()->SetText(newData->GetSql(mainForm->GetBrowser()));
            mainForm->GetSqlPane()->SetReadOnly(true);
        }
    }
    else
    {
        wxTreeItemId collectionItem=item;

        while (collectionItem)
        {
            // search up the tree for our collection
            if (tryUpdate(collectionItem))
                break;
            collectionItem=mainForm->GetBrowser()->GetItemParent(collectionItem);
        }
        /*
        if (!collectionItem)
        {
            // search leaves for our collection
            // shouldn't become necessary
            long cookie;
            collectionItem=mainForm->GetBrowser()->GetFirstChild(item, cookie);

            while (collectionItem)
            {
                if (tryUpdate(collectionItem))
                    break;
                collectionItem=mainForm->GetBrowser()->GetNextChild(item, cookie);
            }
        }
        */
    }
}

        
void dlgProperty::OnOK(wxNotifyEvent &ev)
{

    if (IsModal())
    {
        EndModal(0);
        return;
    }

    wxString sql=GetSql();
    if (!sql.IsEmpty())
    {
        if (!connection->ExecuteVoid(sql))
        {
            // error message is displayed inside ExecuteVoid
            return;
        }

        ShowObject();
    }

    Destroy();
}


void dlgProperty::OnCancel(wxNotifyEvent &ev)
{
    if (IsModal())
        EndModal(-1);
    else
        Destroy();
}



void dlgProperty::OnPageSelect(wxNotebookEvent& event)
{
    if (sqlPane && event.GetSelection() == (int)nbNotebook->GetPageCount()-1)
    {
        sqlPane->SetReadOnly(false);
        if (btnOK->IsEnabled())
            sqlPane->SetText(GetSql());
        else
        {
            if (GetObject())
                sqlPane->SetText(_("-- nothing to change"));
            else
                sqlPane->SetText(_("-- definition incomplete"));
        }
        sqlPane->SetReadOnly(true);
    }
}



dlgProperty *dlgProperty::CreateDlg(frmMain *frame, pgObject *node, bool asNew, int type)
{
    pgConn *conn=node->GetConnection();
    if (!conn)
        return 0;

    if (type < 0)
    {
        type=node->GetType();
        if (node->IsCollection())
            type++;
    }

    pgObject *currentNode, *parentNode;
    if (asNew)
        currentNode=0;
    else
        currentNode=node;

    if (type != node->GetType())
        parentNode = node;
    else
        parentNode = (pgObject*)frame->GetBrowser()->GetItemData(
                                frame->GetBrowser()->GetItemParent(
                                    node->GetId()));

    if (parentNode && parentNode->IsCollection())
        parentNode = (pgObject*)frame->GetBrowser()->GetItemData(
                                frame->GetBrowser()->GetItemParent(
                                    parentNode->GetId()));

    dlgProperty *dlg=0;

    switch (type)
    {
        case PG_USER:
        case PG_USERS:
            dlg=new dlgUser(frame, (pgUser*)currentNode);
            break;
        case PG_GROUP:
        case PG_GROUPS:
            dlg=new dlgGroup(frame, (pgGroup*)currentNode);
            break;
        case PG_DATABASE:
        case PG_DATABASES:
            dlg=new dlgDatabase(frame, (pgDatabase*)currentNode);
            break;
        case PG_CAST:
        case PG_CASTS:
            dlg=new dlgCast(frame, (pgCast*)currentNode);
            break;
        case PG_SCHEMA:
        case PG_SCHEMAS:
            dlg=new dlgSchema(frame, (pgSchema*)currentNode);
            break;
        case PG_LANGUAGE:
        case PG_LANGUAGES:
            dlg=new dlgLanguage(frame, (pgLanguage*)currentNode);
            break;
        case PG_AGGREGATE:
        case PG_AGGREGATES:
            dlg=new dlgAggregate(frame, (pgAggregate*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_CONVERSION:
        case PG_CONVERSIONS:
            dlg=new dlgConversion(frame, (pgConversion*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_DOMAIN:
        case PG_DOMAINS:
            dlg=new dlgDomain(frame, (pgDomain*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_TRIGGERFUNCTION:
            if (parentNode->GetType() == PG_TRIGGER)
                parentNode = ((pgTrigger*)parentNode)->GetSchema();
        case PG_FUNCTION:
        case PG_FUNCTIONS:
        case PG_TRIGGERFUNCTIONS:
            dlg=new dlgFunction(frame, (pgFunction*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_TABLE:
        case PG_TABLES:
            dlg=new dlgTable(frame, (pgTable*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_COLUMN:
        case PG_COLUMNS:
            dlg=new dlgColumn(frame, (pgColumn*)currentNode, (pgTable*)parentNode);
            break;
        case PG_INDEX:
        case PG_INDEXES:
            dlg=new dlgIndex(frame, (pgIndex*)currentNode, (pgTable*)parentNode);
            break;
        case PG_PRIMARYKEY:
            dlg=new dlgPrimaryKey(frame, (pgIndex*)currentNode, (pgTable*)parentNode);
            break;
        case PG_FOREIGNKEY:
            dlg=new dlgForeignKey(frame, (pgForeignKey*)currentNode, (pgTable*)parentNode);
            break;
        case PG_UNIQUE:
            dlg=new dlgUnique(frame, (pgIndex*)currentNode, (pgTable*)parentNode);
            break;
        case PG_CHECK:
            dlg=new dlgCheck(frame, (pgCheck*)currentNode, (pgTable*)parentNode);
            break;
        case PG_SEQUENCE:
        case PG_SEQUENCES:
            dlg=new dlgSequence(frame, (pgSequence*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_TRIGGER:
        case PG_TRIGGERS:
            dlg=new dlgTrigger(frame, (pgTrigger*)currentNode, (pgTable*)parentNode);
            break;
        case PG_TYPE:
        case PG_TYPES:
            dlg=new dlgType(frame, (pgType*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_OPERATOR:
        case PG_OPERATORS:
            dlg=new dlgOperator(frame, (pgOperator*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_VIEW:
        case PG_VIEWS:
            dlg=new dlgView(frame, (pgView*)currentNode, (pgSchema*)parentNode);
            break;
        case PG_RULE:
        case PG_RULES:
            dlg=new dlgRule(frame, (pgRule*)currentNode, (pgTable*)parentNode);
            break;
        case PGA_JOB:
            dlg=new dlgJob(frame, (pgaJob*)currentNode);
            break;
        case PGA_STEP:
            dlg=new dlgStep(frame, (pgaStep*)currentNode, (pgaJob*)parentNode);
            break;
        case PGA_SCHEDULE:
            dlg=new dlgSchedule(frame, (pgaSchedule*)currentNode, (pgaJob*)parentNode);
            break;
        default:
            break;
    }

    if (dlg)
    {
        dlg->CenterOnParent();
        if (dlg->objectType < 0)
            dlg->objectType=type;
        dlg->connection=conn;

        if (type != node->GetType() && !node->IsCollection())
        {
            wxCookieType cookie;
            wxTreeItemId collectionItem=frame->GetBrowser()->GetFirstChild(node->GetId(), cookie);
            while (collectionItem)
            {
                pgCollection *collection=(pgCollection*)frame->GetBrowser()->GetItemData(collectionItem);
                if (collection && collection->IsCollection() && collection->IsCollectionForType(type))
                    break;

                collectionItem=frame->GetBrowser()->GetNextChild(node->GetId(), cookie);
            }
            dlg->item=collectionItem;
        }
        else
            dlg->item=node->GetId();
    }
    return dlg;
}


void dlgProperty::CreateObjectDialog(frmMain *frame, pgObject *node, int type)
{
    dlgProperty *dlg=CreateDlg(frame, node, true, type);

    if (dlg)
    {
        dlg->SetTitle(wxGetTranslation(typesList[dlg->objectType].newString));

        dlg->CreateAdditionalPages();
        dlg->Go();
    }
    else
        wxMessageBox(_("Not implemented."));
}


void dlgProperty::EditObjectDialog(frmMain *frame, ctlSQLBox *sqlbox, pgObject *node)
{
    pgConn *conn=node->GetConnection();
    if (!conn)
        return;

    dlgProperty *dlg=CreateDlg(frame, node, false);

    if (dlg)
    {
        dlg->SetTitle(wxString(wxGetTranslation(typesList[dlg->objectType].typName)) + wxT(" ") + node->GetFullIdentifier());

        dlg->CreateAdditionalPages();
        dlg->Go();
    }
    else
        wxMessageBox(wxT("Not implemented"));
}


void dlgProperty::CreateListColumns(ctlListView *list, const wxString &left, const wxString &right, int leftSize)
{
    int rightSize;
    if (leftSize < 0)
    {
        leftSize = rightSize = list->GetClientSize().GetWidth()/2;
    }
    else
    {
        if (leftSize)
            leftSize = ConvertDialogToPixels(wxPoint(leftSize, 0)).x;
        rightSize = list->GetClientSize().GetWidth()-leftSize;
    }
    if (!leftSize)
    {
        list->InsertColumn(0, left, wxLIST_FORMAT_LEFT, list->GetClientSize().GetWidth());
    }
    else
    {
        list->InsertColumn(0, left, wxLIST_FORMAT_LEFT, leftSize);
        list->InsertColumn(1, right, wxLIST_FORMAT_LEFT, rightSize);
    }
    list->SetImageList(mainForm->GetImageList(), wxIMAGE_LIST_SMALL);
}


int dlgProperty::AppendListItem(ctlListView *list, const wxString& str1, const wxString& str2, int icon)
{
    int pos=list->GetItemCount();
    list->InsertItem(pos, str1, icon);
    if (str2 != wxT(""))
        list->SetItem(pos, 1, str2);
    return pos;
}



/////////////////////////////////////////////////////////////////////////////


dlgTypeProperty::dlgTypeProperty(frmMain *frame, const wxString &resName)
: dlgProperty(frame, resName)
{
    isVarLen=false;
    isVarPrec=false;
    if (wxWindow::FindWindow(XRCID("txtLength")))
    {
        txtLength = CTRL_TEXT("txtLength");
        txtLength->SetValidator(numericValidator);
        txtLength->Disable();
    }
    else
        txtLength = 0;
    if (wxWindow::FindWindow(XRCID("txtPrecision")))
    {
        txtPrecision= CTRL_TEXT("txtPrecision");
        txtPrecision->SetValidator(numericValidator);
        txtPrecision->Disable();
    }
    else
        txtPrecision=0;
}


void dlgTypeProperty::FillDatatype(wxComboBox *cb, bool withDomains)
{
    FillDatatype(cb, 0, withDomains);
}


void dlgTypeProperty::FillDatatype(wxComboBox *cb, wxComboBox *cb2, bool withDomains)
{
    DatatypeReader tr(connection, withDomains);
    while (tr.HasMore())
    {
        pgDatatype dt=tr.GetDatatype();

        AddType(wxT("?"), tr.GetOid(), tr.GetQuotedSchemaPrefix() + dt.QuotedFullName());
        cb->Append(tr.GetSchemaPrefix() + dt.FullName());
        if (cb2)
            cb2->Append(tr.GetSchemaPrefix() + dt.FullName());
        tr.MoveNext();
    }
}


int dlgTypeProperty::Go(bool modal)
{
    if (GetObject())
    {
        if (txtLength)
            txtLength->SetValidator(numericValidator);
        if (txtPrecision)
            txtPrecision->SetValidator(numericValidator);
    }
    return dlgProperty::Go(modal);
}



void dlgTypeProperty::AddType(const wxString &typ, const OID oid, const wxString quotedName)
{
    wxString vartyp;
    if (typ == wxT("?"))
    {
        switch ((long)oid)
        {
            case PGOID_TYPE_BIT:
            case PGOID_TYPE_BIT_ARRAY:
            case PGOID_TYPE_CHAR:
			case PGOID_TYPE_CHAR_ARRAY:
            case PGOID_TYPE_VARCHAR:
			case PGOID_TYPE_VARCHAR_ARRAY:
                vartyp=wxT("L");
                break;
            case PGOID_TYPE_TIME:
			case PGOID_TYPE_TIME_ARRAY:
            case PGOID_TYPE_TIMETZ:
			case PGOID_TYPE_TIMETZ_ARRAY:
            case PGOID_TYPE_TIMESTAMP:
			case PGOID_TYPE_TIMESTAMP_ARRAY:
            case PGOID_TYPE_TIMESTAMPTZ:
			case PGOID_TYPE_TIMESTAMPTZ_ARRAY:
            case PGOID_TYPE_INTERVAL:
			case PGOID_TYPE_INTERVAL_ARRAY:
                vartyp=wxT("D");
                break;
            case PGOID_TYPE_NUMERIC:
			case PGOID_TYPE_NUMERIC_ARRAY:
                vartyp=wxT("P");
                break;
            default:
                vartyp=wxT(" ");
                break;
        }
    }
    else
        vartyp=typ;

    types.Add(vartyp + NumToStr(oid) + wxT(":") + quotedName);
}

    
wxString dlgTypeProperty::GetTypeInfo(int sel)
{
    wxString str;
    if (sel >= 0)
        str = types.Item(sel);

    return str;
}


wxString dlgTypeProperty::GetTypeOid(int sel)
{
    wxString str;
    if (sel >= 0)
        str = types.Item(sel).Mid(1).BeforeFirst(':');

    return str;
}


wxString dlgTypeProperty::GetQuotedTypename(int sel)
{
    wxString sql;
	bool isArray = FALSE;

    if (sel >= 0)
    {
        sql = types.Item(sel).AfterFirst(':');
		if (sql.Contains(wxT("[]"))) {
			sql = sql.BeforeFirst('[');
			isArray = TRUE;
		}

        if (isVarLen && txtLength)
        {
            wxString varlen=txtLength->GetValue();
            if (!varlen.IsEmpty() && NumToStr(StrToLong(varlen))==varlen && StrToLong(varlen) >= minVarLen)
            {
                sql += wxT("(") + varlen;
                if (isVarPrec && txtPrecision)
                {
                    wxString varprec=txtPrecision->GetValue();
                    if (!varprec.IsEmpty())
                        sql += wxT(", ") + varprec;
                }
                sql += wxT(")");
            }
        }
    }

	if (isArray) sql += wxT("[]");
    return sql;
}


void dlgTypeProperty::CheckLenEnable()
{
    int sel=cbDatatype->GetSelection();
    if (sel >= 0)
    {
        wxString info=types.Item(sel);
        isVarPrec = info.StartsWith(wxT("P"));
        isVarLen =  isVarPrec || info.StartsWith(wxT("L")) || info.StartsWith(wxT("D"));
        minVarLen = (info.StartsWith(wxT("D")) ? 0 : 1);
        maxVarLen = isVarPrec ? 1000 : 
                    minVarLen ? 0x7fffffff : 10;
    }
}


/////////////////////////////////////////////////////////////////////////////


dlgCollistProperty::dlgCollistProperty(frmMain *frame, const wxString &resName, pgTable *parentNode)
: dlgProperty(frame, resName)
{
    columns=0;
    table=parentNode;
}


dlgCollistProperty::dlgCollistProperty(frmMain *frame, const wxString &resName, ctlListView *colList)
: dlgProperty(frame, resName)
{
    columns=colList;
    table=0;
}


int dlgCollistProperty::Go(bool modal)
{
    if (columns)
    {
        int pos;
        // iterate cols
        for (pos=0 ; pos < columns->GetItemCount() ; pos++)
        {
            wxString col=columns->GetItemText(pos);
            if (cbColumns->FindString(col) < 0)
                cbColumns->Append(col);
        }
    }
    if (table)
    {
        wxCookieType cookie;
        pgObject *data;
        wxTreeItemId columnsItem=mainForm->GetBrowser()->GetFirstChild(table->GetId(), cookie);
        while (columnsItem)
        {
            data=(pgObject*)mainForm->GetBrowser()->GetItemData(columnsItem);
            if (data->GetType() == PG_COLUMNS)
                break;
            columnsItem=mainForm->GetBrowser()->GetNextChild(table->GetId(), cookie);
        }

        if (columnsItem)
        {
            wxCookieType cookie;
            pgColumn *column;
            wxTreeItemId item=mainForm->GetBrowser()->GetFirstChild(columnsItem, cookie);

            // check columns
            while (item)
            {
                column=(pgColumn*)mainForm->GetBrowser()->GetItemData(item);
                if (column->GetType() == PG_COLUMN)
                {
                    if (column->GetColNumber() > 0)
                    {
                        cbColumns->Append(column->GetName());
                    }
                }
        
                item=mainForm->GetBrowser()->GetNextChild(columnsItem, cookie);
            }
        }
    }
    return dlgProperty::Go(modal);
}



/////////////////////////////////////////////////////////////////////////////


BEGIN_EVENT_TABLE(dlgSecurityProperty, dlgProperty)
    EVT_LIST_ITEM_SELECTED(CTL_LBPRIV,  dlgSecurityProperty::OnPrivSelChange)
    EVT_BUTTON(CTL_ADDPRIV,             dlgSecurityProperty::OnAddPriv)
    EVT_BUTTON(CTL_DELPRIV,             dlgSecurityProperty::OnDelPriv)
    EVT_CHECKBOX(CTL_ALLPRIV,           dlgSecurityProperty::OnPrivCheckAll)
    EVT_CHECKBOX(CTL_ALLPRIVGRANT,      dlgSecurityProperty::OnPrivCheckAllGrant)
    EVT_CHECKBOX(CTL_PRIVCB,            dlgSecurityProperty::OnPrivCheck)
    EVT_CHECKBOX(CTL_PRIVCB+2,          dlgSecurityProperty::OnPrivCheck)
    EVT_CHECKBOX(CTL_PRIVCB+4,          dlgSecurityProperty::OnPrivCheck)
    EVT_CHECKBOX(CTL_PRIVCB+6,          dlgSecurityProperty::OnPrivCheck)
    EVT_CHECKBOX(CTL_PRIVCB+8,          dlgSecurityProperty::OnPrivCheck)
    EVT_CHECKBOX(CTL_PRIVCB+10,         dlgSecurityProperty::OnPrivCheck)
    EVT_CHECKBOX(CTL_PRIVCB+12,         dlgSecurityProperty::OnPrivCheck)
END_EVENT_TABLE();



dlgSecurityProperty::dlgSecurityProperty(frmMain *frame, pgObject *obj, const wxString &resName, const wxString& privilegeList, char *_privChar)
        : dlgProperty(frame, resName)
{
    privCheckboxes=0;
    privilegeChars = _privChar;
    securityChanged=false;
    allPrivileges=0;

    bool needAll=strlen(privilegeChars) > 1;

    wxStringTokenizer privileges(privilegeList, wxT(","));
    privilegeCount=privileges.CountTokens();

    if (privilegeCount)
    {
        wxPanel *page = new wxPanel(nbNotebook, -1, wxDefaultPosition, wxDefaultSize);
        privCheckboxes = new wxCheckBox*[privilegeCount*2];
        int i=0;

        nbNotebook->AddPage(page, _("Security"));

        wxPoint zeroPos=ConvertDialogToPixels(wxPoint(5, 5));
        wxSize chkSize=ConvertDialogToPixels(wxSize(65,12));
        wxSize btnSize=ConvertDialogToPixels(wxSize(50,15));
        wxSize spcSize=ConvertDialogToPixels(wxSize(2, 2));

        if (obj && !obj->CanCreate())
        {
            // We can't create, so we won't be allowed to change privileges either.
            // later, we can check individually too.

            lbPrivileges = new ctlListView(page, CTL_LBPRIV, zeroPos, 
                wxSize(width - zeroPos.x * 2, height - zeroPos.y * 2), wxSUNKEN_BORDER);
            CreateListColumns(lbPrivileges, _("User/Group"), _("Privileges"), -1);
            cbGroups=0;
        }
        else
        {
            int y = height 
                - spcSize.GetHeight()
                - zeroPos.y * 2
                - chkSize.GetHeight() * (4 + privilegeCount + (needAll ? 0 : 1));

            lbPrivileges = new ctlListView(page, CTL_LBPRIV, 
                zeroPos, 
                wxSize(width - zeroPos.x * 2, y - zeroPos.y - spcSize.GetHeight()), wxSUNKEN_BORDER|wxLC_REPORT);

            CreateListColumns(lbPrivileges, _("User/Group"), _("Privileges"), -1);

            btnAddPriv = new wxButton(page, CTL_ADDPRIV, _("Add/Change"), 
                wxPoint(zeroPos.x, y), btnSize);
            btnDelPriv = new wxButton(page, CTL_DELPRIV, _("Remove"), 
                wxPoint(zeroPos.x * 2 + btnSize.GetWidth(), y), btnSize);
            y += zeroPos.y + btnSize.GetHeight();

            new wxStaticBox(page, -1, _("Privileges"), 
                wxPoint(zeroPos.x, y), 
                wxSize(width - zeroPos.x*2, btnSize.GetHeight() + chkSize.GetHeight() * (2 + privilegeCount-(needAll?0:1))));
            y += zeroPos.y + spcSize.GetHeight();

            stGroup = new wxStaticText(page, CTL_STATICGROUP, _("Group"), wxPoint(zeroPos.x * 2, y+3), chkSize);
            cbGroups = new wxComboBox(page, CTL_CBGROUP, wxT(""), 
                wxPoint(zeroPos.x * 3 + chkSize.GetWidth(), y), 
                wxSize(width - zeroPos.x * 4 - chkSize.GetWidth() - spcSize.GetWidth(), chkSize.GetHeight()));
            y += btnSize.GetHeight();

            if (needAll)
            {
                allPrivileges = new wxCheckBox(page, CTL_ALLPRIV, wxT("ALL"), 
                    wxPoint(zeroPos.x * 2, y), 
                    chkSize);
                allPrivilegesGrant = new wxCheckBox(page, CTL_ALLPRIVGRANT, wxT("WITH GRANT OPTION"), 
                    wxPoint(zeroPos.x * 3 + chkSize.GetWidth(), y), 
                    wxSize(width - zeroPos.x * 4 - chkSize.GetWidth() - spcSize.GetWidth(), chkSize.GetHeight()));
                y += chkSize.GetHeight();
                allPrivilegesGrant->Disable();
            }
            cbGroups->Append(wxT("public"));
            cbGroups->SetSelection(0);

            while (privileges.HasMoreTokens())
            {
                wxString priv=privileges.GetNextToken();
                wxCheckBox *cb;
                cb=new wxCheckBox(page, CTL_PRIVCB+i, priv, 
                    wxPoint(zeroPos.x * 2, y), 
                    chkSize);
                privCheckboxes[i++] = cb;
                cb=new wxCheckBox(page, CTL_PRIVCB+i, wxT("WITH GRANT OPTION"), 
                    wxPoint(zeroPos.x * 3 + chkSize.GetWidth(), y), 
                    wxSize(width - zeroPos.x * 4 - chkSize.GetWidth() - spcSize.GetWidth(), chkSize.GetHeight()));
                cb->Disable();
                privCheckboxes[i++] = cb;

                y += chkSize.GetHeight();
            }
        }


        if (obj)
        {
            wxString str=obj->GetAcl();
            if (!str.IsEmpty())
            {
                str = str.Mid(1, str.Length()-2);
                wxStringTokenizer tokens(str, wxT(","));

                while (tokens.HasMoreTokens())
                {
                    wxString str=tokens.GetNextToken();
                    if (str[0U]== '"')
                        str = str.Mid(1, str.Length()-2);

                    wxString name=str.BeforeLast('=');
                    wxString value=str.Mid(name.Length()+1);

                    int icon=PGICON_USER;

                    if (name.Left(6).IsSameAs(wxT("group "), false))
                    {
                        icon = PGICON_GROUP;
                        name = wxT("group ") + name.Mid(6);
                    }
                    else if (name.IsEmpty())
                    {
                        icon = PGICON_PUBLIC;
                        name=wxT("public");
                    }

                    AppendListItem(lbPrivileges, name, value, icon);
                    currentAcl.Add(name + wxT("=") + value);
                }
            }
        }
    }
}


dlgSecurityProperty::~dlgSecurityProperty()
{
    if (privCheckboxes)
        delete[] privCheckboxes;
}



void dlgSecurityProperty::AddGroups(wxComboBox *comboBox)
{
    if (!cbGroups && !comboBox)
        return;

    pgSet *set=connection->ExecuteSet(wxT("SELECT groname FROM pg_group ORDER BY groname"));

    if (set)
    {
        while (!set->Eof())
        {
            if (cbGroups)
                cbGroups->Append(wxT("group ") + set->GetVal(0));
            if (comboBox)
                comboBox->Append(set->GetVal(0));
            set->MoveNext();
        }
        delete set;
    }
}


void dlgSecurityProperty::AddUsers(wxComboBox *comboBox)
{
    if (!comboBox && !cbGroups)
        return;
    
    if (!settings->GetShowUsersForPrivileges() && !comboBox)
        return;

    pgSet *set=connection->ExecuteSet(wxT("SELECT usename FROM pg_user ORDER BY usename"));

    if (set)
    {
        if (cbGroups && settings->GetShowUsersForPrivileges())
            stGroup->SetLabel(_("Group/User"));

        while (!set->Eof())
        {
            if (cbGroups && settings->GetShowUsersForPrivileges())
                cbGroups->Append(set->GetVal(0));

            if (comboBox)
                comboBox->Append(set->GetVal(0));

            set->MoveNext();
        }
        delete set;
    }
}


void dlgSecurityProperty::OnPrivCheckAll(wxCommandEvent& ev)
{
    bool all=allPrivileges->GetValue();
    int i;
    for (i=0 ; i < privilegeCount ; i++)
    {
        if (all)
        {
            privCheckboxes[i*2]->SetValue(true);
            privCheckboxes[i*2]->Disable();
            privCheckboxes[i*2+1]->Disable();
            allPrivilegesGrant->Enable(GrantAllowed());
        }
        else
        {
            allPrivilegesGrant->Disable();
            allPrivilegesGrant->SetValue(false);
            privCheckboxes[i*2]->Enable();
            ExecPrivCheck(i);
        }
    }
}


void dlgSecurityProperty::OnPrivSelChange(wxListEvent &ev)
{
    if (!cbGroups)
        return;
    if (allPrivileges)
    {
        allPrivileges->SetValue(false);
        allPrivilegesGrant->SetValue(false);
        allPrivilegesGrant->Disable();
    }
    long pos=lbPrivileges->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (pos >= 0)
    {
        wxString name=lbPrivileges->GetItemText(pos);
        wxString value=lbPrivileges->GetText(pos, 1);

        pos=cbGroups->FindString(name);
        if (pos < 0)
        {
            cbGroups->Append(name);
            pos=cbGroups->GetCount()-1;
        }
        cbGroups->SetSelection(pos);

        int i;
        for (i=0 ; i < privilegeCount ; i++)
        {
            privCheckboxes[i*2]->Enable();
            int index=value.Find(privilegeChars[i]);
            if (index >= 0)
            {
                privCheckboxes[i*2]->SetValue(true);
                privCheckboxes[i*2+1]->SetValue(value.Mid(index+1, 1) == wxT("*"));
            }
            else
                privCheckboxes[i*2]->SetValue(false);
            ExecPrivCheck(i);
        }
    }
}


void dlgSecurityProperty::OnPrivCheckAllGrant(wxCommandEvent& ev)
{
    bool grant=allPrivilegesGrant->GetValue();
    int i;
    for (i=0 ; i < privilegeCount ; i++)
        privCheckboxes[i*2+1]->SetValue(grant);
}


void dlgSecurityProperty::OnPrivCheck(wxCommandEvent& ev)
{
    int id=(ev.GetId()-CTL_PRIVCB) /2;
    ExecPrivCheck(id);
}


void dlgSecurityProperty::ExecPrivCheck(int id)
{
    bool canGrant=(GrantAllowed() && privCheckboxes[id*2]->GetValue());
    if (canGrant)
        privCheckboxes[id*2+1]->Enable();
    else
    {
        privCheckboxes[id*2+1]->SetValue(false);
        privCheckboxes[id*2+1]->Disable();
    }
}


void dlgSecurityProperty::OnAddPriv(wxNotifyEvent &ev)
{
    wxString name=cbGroups->GetValue();

    long pos=lbPrivileges->FindItem(-1, name);
    if (pos < 0)
    {
        pos = lbPrivileges->GetItemCount();
        int icon=PGICON_USER;

        if (name.Left(6).IsSameAs(wxT("group "), false))
            icon = PGICON_GROUP;
        else if (name.IsSameAs(wxT("public"), false))
            icon = PGICON_PUBLIC;

        lbPrivileges->InsertItem(pos, name, icon);
    }
    wxString value;
    int i;
    for (i=0 ; i < privilegeCount ; i++)
    {
        if (privCheckboxes[i*2]->GetValue())
        {
            value += privilegeChars[i];
            if (privCheckboxes[i*2+1]->GetValue())
                value += '*';
        }
    }
    lbPrivileges->SetItem(pos, 1, value);
    securityChanged=true;
    EnableOK(btnOK->IsEnabled());
}


void dlgSecurityProperty::OnDelPriv(wxNotifyEvent &ev)
{
    lbPrivileges->DeleteItem(lbPrivileges->GetSelection());
    securityChanged=true;
    EnableOK(btnOK->IsEnabled());
}


wxString dlgSecurityProperty::GetHelpPage() const
{
    if (nbNotebook->GetSelection() == (int)nbNotebook->GetPageCount()-2)
        return wxT("sql-grant");
    else
        return dlgProperty::GetHelpPage();
}


void dlgSecurityProperty::EnableOK(bool enable)
{
    if (securityChanged)
    {
        wxString sql=GetSql();
        if (sql.IsEmpty())
        {
            enable=false;
            securityChanged=false;
        }
        else
            enable=true;
    }
    dlgProperty::EnableOK(enable);
}


bool dlgSecurityProperty::GrantAllowed() const
{
    if (!connection->BackendMinimumVersion(7, 4))
        return false;

    wxString user=cbGroups->GetValue();
    if (user.Left(6).IsSameAs(wxT("group "), false) || user.IsSameAs(wxT("public"), false))
        return false;

    return true;
}


wxString dlgSecurityProperty::GetGrant(const wxString &allPattern, const wxString &grantObject)
{
    wxString sql;

    wxArrayString tmpAcl=currentAcl;

    int cnt=lbPrivileges->GetItemCount();
    int pos;
    unsigned int i;

    for (pos=0 ; pos < cnt ; pos++)
    {
        wxString name=lbPrivileges->GetText(pos);
        wxString value=lbPrivileges->GetText(pos, 1);

        int nameLen=name.Length();

        bool privWasAssigned=false;
        bool privPartiallyAssigned=false;
        for (i=0 ; i < tmpAcl.GetCount() ; i++)
        {
            if (tmpAcl.Item(i).Left(nameLen) == name)
            {
                privPartiallyAssigned=true;
                if (tmpAcl.Item(i).Mid(nameLen+1) == value)
                    privWasAssigned=true;
                tmpAcl.RemoveAt(i);
                break;
            }
        }

        if (name.Left(6).IsSameAs(wxT("group "), false))
            name = wxT("GROUP ") + qtIdent(name.Mid(6));
        else
            name=qtIdent(name);

        if (!privWasAssigned)
        {
            if (privPartiallyAssigned)
                sql += pgObject::GetPrivileges(allPattern, wxT(""), grantObject, name);
            sql += pgObject::GetPrivileges(allPattern, value, grantObject, name);
        }
    }

    for (i=0 ; i < tmpAcl.GetCount() ; i++)
    {
        wxString name=tmpAcl.Item(i).BeforeLast('=');

        if (name.Left(6).IsSameAs(wxT("group "), false))
            name = wxT("GROUP ") + qtIdent(name.Mid(6));
        else
            name=qtIdent(name);
        sql += pgObject::GetPrivileges(allPattern, wxT(""), grantObject, name);
    }
    return sql;
}


/////////////////////////////////////////////////////////////////////////////


BEGIN_EVENT_TABLE(dlgOidProperty, dlgProperty)
    EVT_BUTTON (XRCID("btnOK"),                     dlgOidProperty::OnOK)
END_EVENT_TABLE();

dlgOidProperty::dlgOidProperty(frmMain *frame, const wxString &resName)
: dlgProperty(frame, resName)
{
    oid=0;
}


wxString dlgOidProperty::GetSql()
{
    wxString str=GetInsertSql();
    if (!str.IsEmpty())
        str += wxT("\n\n");
    return str + GetUpdateSql();
}



bool dlgOidProperty::executeSql()
{
    wxString sql;
    bool dataChanged=false;

    sql=GetInsertSql();
    if (!sql.IsEmpty())
    {
        pgSet *set=connection->ExecuteSet(sql);
        if (set)
        {
            oid = set->GetInsertedOid();
            delete set;
        }
        if (!set || !oid)
        {
            return false;
        }
        dataChanged=true;
    }

    sql=GetUpdateSql();
    if (!sql.IsEmpty())
    {
        int pos;
        while ((pos=sql.Find(wxT("<Oid>"))) >= 0)
            sql = sql.Left(pos) + NumToStr(oid) + wxT("::oid") + sql.Mid(pos+5);

        if (!connection->ExecuteVoid(sql))
        {
            // error message is displayed inside ExecuteVoid
            return false;
        }
        dataChanged=true;
    }

    return dataChanged;
}


void dlgOidProperty::OnOK(wxNotifyEvent &ev)
{
    if (IsModal())
    {
        EndModal(0);
        return;
    }

    

    connection->ExecuteVoid(wxT("BEGIN TRANSACTION"));

    if (executeSql())
    {
        connection->ExecuteVoid(wxT("COMMIT TRANSACTION"));
        ShowObject();
    }
    else
    {
        connection->ExecuteVoid(wxT("ROLLBACK TRANSACTION"));
    }

    Destroy();
}