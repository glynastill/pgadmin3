//////////////////////////////////////////////////////////////////////////
//
// pgAdmin III - PostgreSQL Tools
// RCS-ID:      $Id$
// Copyright (C) 2002 - 2003, The pgAdmin Development Team
// This software is released under the Artistic Licence
//
// dlgSchedule.cpp - PostgreSQL Schedule Property
//
//////////////////////////////////////////////////////////////////////////

#include "pgAdmin3.h"

// wxWindows headers
#include <wx/wx.h>
#include "calbox.h"
#include "timespin.h"

// App headers
#include "misc.h"
#include "dlgSchedule.h"
#include "pgaSchedule.h"

// Images
#include "images/schedule.xpm"


// pointer to controls
#define chkEnabled          CTRL("chkEnabled", wxCheckBox)
#define cbKind              CTRL("cbKind", wxComboBox)
#define timInterval         CTRL("timInterval", wxTimeSpinCtrl)
#define calStart            CTRL("calStart", wxCalendarBox)
#define timStart            CTRL("timStart", wxTimeSpinCtrl)
#define calEnd              CTRL("calEnd", wxCalendarBox)
#define timEnd              CTRL("timEnd", wxTimeSpinCtrl)
#define calSchedule         CTRL("calSchedule", wxCalendarBox)
#define timSchedule         CTRL("timSchedule", wxTimeSpinCtrl)


BEGIN_EVENT_TABLE(dlgSchedule, dlgOidProperty)
    EVT_TEXT(XRCID("txtName"),                      dlgSchedule::OnChange)
    EVT_CHECKBOX(XRCID("chkEnabled"),               dlgSchedule::OnChange)
    EVT_COMBOBOX(XRCID("cbKind"),                   dlgSchedule::OnChange)
    EVT_SPIN(XRCID("timInterval"),                  dlgSchedule::OnChange)
    EVT_CALENDAR_SEL_CHANGED(XRCID("calStart"),     dlgSchedule::OnChange)
    EVT_SPIN(XRCID("timStart"),                     dlgSchedule::OnChange)
    EVT_CALENDAR_SEL_CHANGED(XRCID("calEnd"),       dlgSchedule::OnChange)
    EVT_SPIN(XRCID("timEnd"),                       dlgSchedule::OnChange)
    EVT_CALENDAR_SEL_CHANGED(XRCID("calSchedule"),  dlgSchedule::OnChange)
    EVT_SPIN(XRCID("timSchedule"),                  dlgSchedule::OnChange)
    EVT_TEXT(XRCID("txtComment"),                   dlgSchedule::OnChange)
END_EVENT_TABLE();


dlgSchedule::dlgSchedule(frmMain *frame, pgaSchedule *node, pgaJob *j)
: dlgOidProperty(frame, wxT("dlgSchedule"))
{
    SetIcon(wxIcon(schedule_xpm));
    schedule=node;
    job=j;
    if (job)
        jobOid=job->GetOid();
    else
        jobOid=0;

    timInterval->SetMax(365*24*60*60 -1, true);
    txtOID->Disable();
}


pgObject *dlgSchedule::GetObject()
{
    return schedule;
}


int dlgSchedule::Go(bool modal)
{
    if (schedule)
    {
        // edit mode
        txtName->SetValue(schedule->GetName());
        txtOID->SetValue(NumToStr((long)schedule->GetOid()));
        chkEnabled->SetValue(schedule->GetEnabled());
        cbKind->SetSelection(wxString(wxT("nsdwmy")).Find(schedule->GetKindChar()));
        calStart->SetDate(schedule->GetStart());
        timStart->SetTime(schedule->GetStart());
        if (schedule->GetEnd().IsValid())
        {
            calEnd->SetDate(schedule->GetEnd());
            timEnd->SetTime(schedule->GetEnd());
        }
        else
            timEnd->Disable();
        calSchedule->SetDate(schedule->GetSchedule());
        timSchedule->SetTime(schedule->GetSchedule());
        timInterval->SetValue(schedule->GetIntervalList().Item(0));

        txtComment->SetValue(schedule->GetComment());
    }
    else
    {
        // create mode
    }

    return dlgProperty::Go(modal);
}


pgObject *dlgSchedule::CreateObject(pgCollection *collection)
{
    wxString name=GetName();

    pgObject *obj=pgaSchedule::ReadObjects(job, 0, wxT("   AND sc.oid=") + NumToStr(oid) + wxT("\n"));
    return obj;
}


void dlgSchedule::OnChange(wxNotifyEvent &ev)
{
    timEnd->Enable(calEnd->GetDate().IsValid());

    wxString name=GetName();
    bool enable;
    if (schedule)
    {
        enable  =  name != schedule->GetName()
                || chkEnabled->GetValue() != schedule->GetEnabled()
                || cbKind->GetSelection() != wxString(wxT("nsdwmy")).Find(schedule->GetKindChar())
                || txtComment->GetValue() != schedule->GetComment();
    }
    else
    {
        enable=true;
    }
    CheckValid(enable, !name.IsEmpty(), _("Please specify name."));
    EnableOK(enable);
}




wxString dlgSchedule::GetInsertSql()
{
    wxString sql;
    if (!schedule)
    {
        wxString name=GetName();
        wxString kind = wxT("nsdwmy")[cbKind->GetSelection()];
        wxString jstjoboid;
        if (jobOid)
            jstjoboid = NumToStr(jobOid);
        else
            jstjoboid = wxT("<JobOid>");
#if 0

        sql = wxT("INSERT INTO pgXXXXXXXXXXXa_jobschedule (jstjoboid, jstname, jstdesc, jstenabled, jstkind, jstonerror, jstcode, jstdboid)\n")
              wxT("SELECT ") + jstjoboid + wxT(", ") + qtString(name) + wxT(", ") + qtString(txtComment->GetValue()) + wxT(", ")
                + BoolToStr(chkEnabled->GetValue()) + wxT(", ") + qtString(kind) + wxT(", ") 
                + qtString(onerror) + wxT(", ") + qtString(sqlBox->GetText()) + wxT(", ") + db;
#endif
    }
    return sql;
}


wxString dlgSchedule::GetUpdateSql()
{
    wxString sql, name;
    name=GetName();

    if (job)
    {
        // edit mode
    }
    else
    {
        // create mode

    }
    return sql;
}
