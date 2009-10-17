/**
 * Copyright (C) 2008  Ellis Whitehead
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "MainScope.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>

#include "AppDefines.h"
#include "Check.h"
#include "Globals.h"
#include "MainScopeUi.h"
#include "RecordHandler.h"
#include "ViewSettings.h"
#include <Idac/IdacProxy.h>
#include <IdacDriver/IdacSettings.h>


MainScope::MainScope(MainScopeUi* ui, IdacProxy* idac, QObject* parent)
	: QObject(parent)
{
	Q_ASSERT(ui != NULL);

	m_ui = ui;
	m_idac = idac;
	connect(m_idac, SIGNAL(isAvailableChanged(bool)), this, SLOT(on_idac_isAvailable()));

	m_taskType = EadTask_Review;
	m_viewType = EadView_Averages;
	m_peakMode = EadPeakMode_Verified;
	m_nPeakModeRecId = 0;

	m_actions = new Actions(this);

	m_file = new EadFile;

	m_bRecentFilesMenuEnabled = false;
	m_bWindowModified = false;

	m_bRecording = false;
	m_vwiEad = NULL;
	m_vwiFid = NULL;
	m_vwiDig = NULL;
	m_recHandler = (m_idac != NULL) ? new RecordHandler(m_idac) : NULL;
	m_recTimer = new QTimer(this);
	m_recTimer->setInterval(200); // 200ms
	connect(m_recTimer, SIGNAL(timeout()), this, SLOT(on_recTimer_timeout()));

	updateActions();
	updateWindowTitle();

	Globals->readSettings();

	updateRecentFileActions();
	m_actions->viewWaveComments->setChecked(Globals->viewSettings()->bShowWaveComments);

	connect(m_actions->fileNew, SIGNAL(triggered()), this, SLOT(on_actions_fileNew_triggered()));
	connect(m_actions->fileOpen, SIGNAL(triggered()), this, SLOT(on_actions_fileOpen_triggered()));
	foreach (QAction* act, m_actions->fileOpenRecentActions)
		connect(act, SIGNAL(triggered()), this, SLOT(on_actions_fileOpenRecentActions_triggered()));
	connect(m_actions->fileSave, SIGNAL(triggered()), this, SLOT(on_actions_fileSave_triggered()));
	connect(m_actions->fileSaveAs, SIGNAL(triggered()), this, SLOT(on_actions_fileSaveAs_triggered()));
	connect(m_actions->fileComment, SIGNAL(triggered()), this, SLOT(on_actions_fileComment_triggered()));
	//connect(m_actions->file, SIGNAL(triggered()), this, SLOT(on_actions_file()));
	//connect(m_actions->file, SIGNAL(triggered()), this, SLOT(on_actions_file()));
	connect(m_actions->fileLoadSampleProject, SIGNAL(triggered()), this, SLOT(on_actions_fileLoadSampleProject_triggered()));
	connect(m_actions->viewViewMode, SIGNAL(triggered()), this, SLOT(on_actions_viewViewMode_triggered()));
	connect(m_actions->viewPublishMode, SIGNAL(triggered()), this, SLOT(on_actions_viewPublishMode_triggered()));
	connect(m_actions->viewChartAverages, SIGNAL(triggered()), this, SLOT(on_actions_viewChartAverages_triggered()));
	connect(m_actions->viewChartEads, SIGNAL(triggered()), this, SLOT(on_actions_viewChartEads_triggered()));
	connect(m_actions->viewChartFids, SIGNAL(triggered()), this, SLOT(on_actions_viewChartFids_triggered()));
	connect(m_actions->viewChartAll, SIGNAL(triggered()), this, SLOT(on_actions_viewChartAll_triggered()));
	connect(m_actions->viewChartRecording, SIGNAL(triggered()), this, SLOT(on_actions_viewChartRecording_triggered()));
	connect(m_actions->viewWaveComments, SIGNAL(triggered()), this, SLOT(on_actions_viewWaveComments_triggered()));
	connect(m_actions->viewHidePeaks, SIGNAL(triggered()), this, SLOT(on_actions_viewHidePeaks_triggered()));
	connect(m_actions->viewDetectedPeaks, SIGNAL(triggered()), this, SLOT(on_actions_viewDetectedPeaks_triggered()));
	connect(m_actions->viewVerifiedPeaks, SIGNAL(triggered()), this, SLOT(on_actions_viewVerifiedPeaks_triggered()));
	connect(m_actions->viewEditPeaks, SIGNAL(triggered()), this, SLOT(on_actions_viewEditPeaks_triggered()));
	connect(m_actions->recordRecord, SIGNAL(triggered()), this, SLOT(on_actions_recordRecord_triggered()));
	connect(m_actions->recordHardwareSettings, SIGNAL(triggered()), this, SLOT(on_actions_recordHardwareSettings_triggered()));
	connect(m_actions->recordSave, SIGNAL(triggered()), this, SLOT(on_actions_recordSave_triggered()));
	connect(m_actions->recordDiscard, SIGNAL(triggered()), this, SLOT(on_actions_recordDiscard_triggered()));
	connect(m_actions->recordHardwareConnect, SIGNAL(triggered()), this, SLOT(on_actions_recordHardwareConnect_triggered()));
}

MainScope::~MainScope()
{
	delete m_ui;
	delete m_file;
	delete m_recHandler;
}

void MainScope::setFile(EadFile* file)
{
	CHECK_PRECOND_RET(m_bRecording == false);

	if (m_file != file)
	{
		delete m_file;
		m_file = file;
		if (m_file != NULL)
		{
			connect(m_file, SIGNAL(dirtyChanged()), this, SLOT(on_file_dirtyChanged()));
			connect(m_file, SIGNAL(waveListChanged()), this, SIGNAL(waveListChanged()));
		}

		emit fileChanged(m_file);

		updateWindowTitle();
		setTaskType(EadTask_Review);
		setViewType(EadView_Averages);
		updateActions();
		addRecentFile(m_file->filename());
		m_actions->viewChartRecording->setEnabled(false);
	}
}

void MainScope::setTaskType(EadTask taskType)
{
	if (taskType != m_taskType)
	{
		m_taskType = taskType;

		m_actions->viewViewMode->setChecked(m_taskType == EadTask_Review);
		m_actions->viewPublishMode->setChecked(m_taskType == EadTask_Publish);

		updateActions();

		emit taskTypeChanged(m_taskType);
	}
}

void MainScope::setViewType(EadView viewType)
{
	if (viewType != m_viewType)
	{
		m_viewType = viewType;

		m_actions->viewChartAverages->setChecked(m_viewType == EadView_Averages);
		m_actions->viewChartEads->setChecked(m_viewType == EadView_EADs);
		m_actions->viewChartFids->setChecked(m_viewType == EadView_FIDs);
		m_actions->viewChartAll->setChecked(m_viewType == EadView_All);
		m_actions->viewChartRecording->setChecked(m_viewType == EadView_Recording);

		updateActions();
		emit viewTypeChanged(m_viewType);
	}
}

void MainScope::setComment(const QString& s)
{
	if (s != m_file->comment())
	{
		m_file->setComment(s);
		emit commentChanged(m_file->comment());
	}
}

void MainScope::setPeakMode(EadPeakMode peakMode)
{
	m_peakMode = peakMode;
	emit peakModeChanged(peakMode);
}

void MainScope::setPeakModeRecId(int id)
{
	m_nPeakModeRecId = id;
	emit peakModeChanged(m_peakMode);
}

void MainScope::setIsRecentFilesMenuEnabled(bool bEnabled)
{
	if (bEnabled != m_bRecentFilesMenuEnabled)
	{
		m_bRecentFilesMenuEnabled = bEnabled;
		emit isRecentFilesMenuEnabledChanged(m_bRecentFilesMenuEnabled);
	}
}

void MainScope::setIsWindowModified(bool bWindowModified)
{
	if (bWindowModified != m_bWindowModified)
	{
		m_bWindowModified = bWindowModified;
		emit isWindowModifiedChanged(m_bWindowModified);
	}
}

void MainScope::setIsRecording(bool bRecording)
{
	if (bRecording != m_bRecording)
	{
		m_bRecording = bRecording;
		emit isRecordingChanged(m_bRecording);

		if (m_bRecording)
			m_recTimer->start();
		else
			m_recTimer->stop();

		updateActions();
	}
}

void MainScope::updateActions()
{
	bool bHaveFile = (m_file != NULL);
	bool bHaveData = (bHaveFile && m_file->recs().count() > 1);

	// Can save a dirty file or one that hasn't been saved yet (even it it's empty)
	m_actions->fileNew->setEnabled(!m_bRecording);
	m_actions->fileOpen->setEnabled(!m_bRecording);
	m_actions->fileSave->setEnabled(bHaveFile && (m_file->isDirty() || !bHaveData));
	m_actions->fileSaveAs->setEnabled(bHaveFile);
	m_actions->fileExportSignalData->setEnabled(bHaveData);
	m_actions->fileExportRetentionData->setEnabled(bHaveData);
	m_actions->fileLoadSampleProject->setEnabled(!m_bRecording);

	bool bView = (m_taskType == EadTask_Review);
	m_actions->viewWaveComments->setEnabled(bView);
	bool bPeaks = (bView && m_viewType != EadView_EADs);
	m_actions->viewHidePeaks->setEnabled(bPeaks);
	m_actions->viewDetectedPeaks->setEnabled(bPeaks);
	m_actions->viewVerifiedPeaks->setEnabled(bPeaks);
	m_actions->viewEditPeaks->setEnabled(bPeaks);

	m_actions->recordRecord->setEnabled(bHaveFile && !m_bRecording);
	m_actions->recordHardwareSettings->setEnabled(bHaveFile && !m_bRecording);
	m_actions->recordSave->setEnabled(bHaveFile && m_bRecording);
	m_actions->recordDiscard->setEnabled(bHaveFile && m_bRecording);
	m_actions->recordHardwareConnect->setEnabled(!m_idac->isAvailable());
}

void MainScope::updateWindowTitle()
{
	QString s;
	bool bDirty;
	
	if (m_file == NULL)
	{
		// Show "APPNAME"
		s = tr(APPNAME);
		bDirty = false;
	}
	else
	{
		QString sFilename;
		if (m_file != NULL)
			sFilename = m_file->filename();
		QString sProject = QFileInfo(sFilename).fileName();
		
		if (sFilename.isEmpty())
			sProject = tr("(New Project)");

		// Show "Filename - APPNAME"
		s = tr("[*]%1 - %2").arg(sProject).arg(tr(APPNAME));

		bDirty = m_file->isDirty();
	}

	if (s != m_sWindowTitle)
	{
		m_sWindowTitle = s;
		emit windowTitleChanged(m_sWindowTitle);
	}
	setIsWindowModified(bDirty);
}

void MainScope::addRecentFile(const QString& sFilename)
{
	if (!sFilename.isEmpty())
	{
		QFileInfo fi(sFilename);
		QString sFullPath = fi.absoluteFilePath();
		
		Globals->recentFiles.removeAll(sFullPath);
		Globals->recentFiles.push_front(sFullPath);
		
		// Don't let the number of recent files exceed the number of actions available
		while (Globals->recentFiles.count() > m_actions->fileOpenRecentActions.count())
			Globals->recentFiles.removeLast();
		
		// Update the actions
		updateRecentFileActions();
	}
}

void MainScope::updateRecentFileActions()
{
	CHECK_PRECOND_RET(Globals->recentFiles.count() < m_actions->fileOpenRecentActions.count());

	// Enable the recent files menu, if appropriate
	setIsRecentFilesMenuEnabled(Globals->recentFiles.count() > 0);
	
	for (int i = 0; i < Globals->recentFiles.count(); i++)
	{
		QString sFilename = Globals->recentFiles[i];
		sFilename = QDir::convertSeparators(sFilename);
		sFilename = QFileInfo(sFilename).fileName();
		QString text = tr("&%1 %2").arg(i + 1).arg(sFilename);
		m_actions->fileOpenRecentActions[i]->setText(text);
		m_actions->fileOpenRecentActions[i]->setVisible(true);
	}
}

bool MainScope::checkHardware()
{
	if (m_idac->isAvailable())
		return true;
	else
		return m_ui->waitForHardware(m_idac, true);
}

void MainScope::on_idac_isAvailable()
{
	if (m_idac->isAvailable())
	{
		m_idac->loadDefaultChannelSettings(Globals->idacSettings()->channels);
		Globals->readIdacChannelSettings(m_idac->hardwareName());
	}
	updateActions();
}

void MainScope::on_file_dirtyChanged()
{
	updateWindowTitle();
	updateActions();
}

void MainScope::on_actions_fileNew_triggered()
{
	if (checkSaveAndContinue())
	{
		EadFile* file = new EadFile;
		setFile(file);
	}
}

void MainScope::on_actions_fileOpen_triggered()
{
	if (checkSaveAndContinue())
	{
		QString sFilename = m_ui->getFileOpenFilename(Globals->lastDir());
		if (!sFilename.isEmpty())
		{
			Globals->setLastDir(QFileInfo(sFilename).absolutePath());
			open(sFilename);
		}
	}
}

void MainScope::open(const QString& _sFilename)
{
	QString sFilename = _sFilename;

	QFileInfo fi(sFilename);
	if (!fi.exists())
	{
		m_ui->showWarning(tr("Project file could not be found:\n%1").arg(_sFilename));
		m_ui->showStatusMessage(tr("Project file not found"));
		return;
	}
	
	sFilename = fi.absoluteFilePath();
	
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));  // HACK: Calling QApplication here is a bad hack -- ellis, 2008-09-15
	EadFile* file = new EadFile;
	LoadSaveResult result = file->load(sFilename);
	QApplication::restoreOverrideCursor();

	if (result == LoadSaveResult_Ok)
	{
		setFile(file);
	}
	else
	{
		delete file;
		m_ui->showWarning(tr("Project file could not be properly loaded:\n%1").arg(_sFilename));
		m_ui->showStatusMessage(tr("Error loading project"));
	}
}

void MainScope::on_actions_fileOpenRecentActions_triggered()
{
	QAction* action = dynamic_cast<QAction*>(sender());
	Q_ASSERT(action != NULL);
	
	if (checkSaveAndContinue())
	{
		int i = action->data().toInt();
		open(Globals->recentFiles[i]);
	}
}

bool MainScope::on_actions_fileSave_triggered()
{
	CHECK_PRECOND_RETVAL(m_file != NULL, false);

	if (m_file->filename().isEmpty())
		return on_actions_fileSaveAs_triggered();
	else
		return save(m_file->filename());
}

bool MainScope::on_actions_fileSaveAs_triggered()
{
	QString sFilename = m_ui->getFileSaveAsFilename(Globals->lastDir());
	if (sFilename.isEmpty())
		return false;

	Globals->setLastDir(QFileInfo(sFilename).absolutePath());

	if (!sFilename.toLower().endsWith(".ead"))
		sFilename += ".ead";

	return save(sFilename);
}

bool MainScope::save(const QString& sFilename)
{
	CHECK_PRECOND_RETVAL(m_file != NULL, false);

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	bool bOk = m_file->saveAs(sFilename);
	QApplication::restoreOverrideCursor();

	if (bOk)
	{
		m_ui->showStatusMessage(tr("Recordings saved"));
		addRecentFile(m_file->filename());
	}
	else
	{
		m_ui->showError(
			tr("Error Saving File"),
			tr("The recordings could not be saved to disk! Please try saving under a different filename or at a different location."));
	}

	updateWindowTitle();

	return bOk;
}

bool MainScope::checkSaveAndContinue()
{
	// If there are unsaved changes to the project
	if (isWindowModified())
	{
		QMessageBox::StandardButton ret = m_ui->warnAboutUnsavedChanged();
		// Save
		if (ret == QMessageBox::Save)
			return on_actions_fileSave_triggered();
		// Cancel
		else if (ret == QMessageBox::Cancel)
			return false;
	}
	return true;
}

void MainScope::on_actions_fileComment_triggered()
{
	QString s = m_ui->getComment(m_file->comment());
	setComment(s);
}

void MainScope::on_actions_fileLoadSampleProject_triggered()
{
	if (checkSaveAndContinue())
	{
		EadFile* file = new EadFile;
		file->createFakeData();
		setFile(file);
	}
}

void MainScope::on_actions_viewViewMode_triggered() { setTaskType(EadTask_Review); }
void MainScope::on_actions_viewPublishMode_triggered() { setTaskType(EadTask_Publish); }

void MainScope::on_actions_viewChartAverages_triggered() { setViewType(EadView_Averages); }
void MainScope::on_actions_viewChartEads_triggered() { setViewType(EadView_EADs); }
void MainScope::on_actions_viewChartFids_triggered() { setViewType(EadView_FIDs); }
void MainScope::on_actions_viewChartAll_triggered() { setViewType(EadView_All); }
void MainScope::on_actions_viewChartRecording_triggered() { setViewType(EadView_Recording); }

void MainScope::on_actions_viewWaveComments_triggered()
{
	Globals->viewSettings()->bShowWaveComments = m_actions->viewWaveComments->isChecked();
	emit viewSettingChanged("bShowWaveComments");
}

void MainScope::on_actions_viewHidePeaks_triggered()
{
	setPeakMode(EadPeakMode_Hide);
}

void MainScope::on_actions_viewDetectedPeaks_triggered()
{
	setPeakMode(EadPeakMode_Detected);
}

void MainScope::on_actions_viewVerifiedPeaks_triggered()
{
	setPeakMode(EadPeakMode_Verified);
}

void MainScope::on_actions_viewEditPeaks_triggered()
{
	setPeakMode(EadPeakMode_Edit);
}

void MainScope::on_actions_recordRecord_triggered()
{
	CHECK_PRECOND_RET(m_file != NULL);
	CHECK_PRECOND_RET(m_idac != NULL);

	if (!checkHardware())
		return;

	if (m_ui->showRecordPreview(m_idac))
	{
		m_file->discardNewRecording();

		RecInfo* rec = m_file->createNewRecording();
		ViewInfo* view = m_file->viewInfo(EadView_Recording);
		// Update the "Recording" view & save pointers for convenience
		view->clearWaves();
		m_vwiEad = view->addWave(rec->ead());
		m_vwiFid = view->addWave(rec->fid());
		m_vwiDig = view->addWave(rec->digital());

		// Set time of recording to now
		rec->setTimeOfRecording(QDateTime::currentDateTime());

		int nDelaySamplesFid = Globals->idacSettings()->nGcDelay_ms / (1000 / EAD_SAMPLES_PER_SECOND);
		m_vwiFid->setShift(-nDelaySamplesFid);

		m_recHandler->updateRawToVoltageFactors();

		// Enable and switch to the Recording view
		m_actions->viewChartRecording->setEnabled(true);
		setTaskType(EadTask_Review);
		setViewType(EadView_Recording);

		setIsRecording(true);
	}
}

void MainScope::on_actions_recordHardwareSettings_triggered()
{
	if (!checkHardware())
		return;

	m_ui->showRecordOptions(m_idac);
	Globals->writeIdacChannelSettings(m_idac->hardwareName());
}

void MainScope::on_actions_recordSave_triggered()
{
	stopRecording(true, false);
}

void MainScope::on_actions_recordDiscard_triggered()
{
	QMessageBox::StandardButton btn = m_ui->question(
		tr("Discard"),
		tr("Are you sure you want to discard the current recording?"),
		QMessageBox::Discard | QMessageBox::Cancel,
		QMessageBox::Discard);

	if (btn == QMessageBox::Discard)
		stopRecording(false, false);
}

void MainScope::on_actions_recordHardwareConnect_triggered()
{
	switch (m_idac->state())
	{
	case IdacState_None:
	case IdacState_Searching:
	case IdacState_NotPresent:
	case IdacState_Present:
	case IdacState_Initializing:
	case IdacState_InitError:
		m_idac->setup();
		m_ui->waitForHardware(m_idac, false);
		break;
	default:
		break;
	};
}

void MainScope::stopRecording(bool bSave, bool bAutoStop)
{
	CHECK_PRECOND_RET(m_file != NULL);
	CHECK_PRECOND_RET(m_bRecording);

	m_idac->stopSampling();
	setIsRecording(false);

	if (bSave)
	{
		m_file->saveNewRecording();

		// Switch to the recording view in the review task
		setTaskType(EadTask_Review);
		setViewType(EadView_Recording);

		bool bSaved = on_actions_fileSave_triggered();

		// Choose an appropriate message:
		QString s;
		if (bAutoStop)
		{
			if (bSaved)
				s = tr("The preset recording time has ellapsed, and your data has been automatically saved.");
			else
				m_ui->showWarning(tr("The preset recording time has ellapsed, but your data has not yet been saved to disk!"));
		}
		else
		{
			if (bSaved)
				s = tr("The new recording has been saved");
			else
				m_ui->showWarning(tr("WARNING: Your data has not yet been saved to disk!"));
		}

		if (!s.isEmpty())
			m_ui->showInformation(tr("Recording Finished"), s);
	}
	else
	{
		// Clear references
		m_vwiEad = NULL;
		m_vwiFid = NULL;
		m_vwiDig = NULL;

		// Delete ViewWaveInfos
		if (m_file != NULL)
		{
			ViewInfo* view = m_file->viewInfo(EadView_Recording);
			view->clearWaves();
			// Delete RecInfo/WaveInfo
			m_file->discardNewRecording();
			emit waveListChanged();
		}

		emit updateRecordings();
	}
}

void MainScope::on_recTimer_timeout()
{
	if (!m_recHandler->check() || !m_recHandler->convert())
		return;

	/*if (!m_bDataReceived)
	{
		m_bDataReceived = true;
		on_spnMag_valueChanged();
		updateStatus();
	}*/

	// Process digital signals (and handle trigger)
	const QVector<short>& digital = m_recHandler->digitalRaw();
	for (int i = 0; i < digital.size(); i++)
	{
		short n = digital[i];
		bool b = ((n & 0x02) != 0);
		WaveInfo* wave = m_vwiDig->waveInfo();
		wave->raw << ((b) ? 1 : -1);
		wave->display << ((b) ? 0.5 : -0.5);
	}

	WaveInfo* wave;
	// Display EAD data
	wave = m_vwiEad->waveInfo();
	wave->raw << m_recHandler->eadRaw();
	wave->display << m_recHandler->eadDisplay();
	m_recHandler->calcRawToVoltageFactors(1, wave->nRawToVoltageFactorNum, wave->nRawToVoltageFactorDen);
	wave->nRawToVoltageFactor = double(wave->nRawToVoltageFactorNum) / wave->nRawToVoltageFactorDen;
	// Display FID data
	wave = m_vwiFid->waveInfo();
	wave->raw << m_recHandler->fidRaw();
	wave->display << m_recHandler->fidDisplay();
	m_recHandler->calcRawToVoltageFactors(2, wave->nRawToVoltageFactorNum, wave->nRawToVoltageFactorDen);
	wave->nRawToVoltageFactor = double(wave->nRawToVoltageFactorNum) / wave->nRawToVoltageFactorDen;

	// Check for end of recording
	if (Globals->idacSettings()->nRecordingDuration > 0)
	{
		int nSamples = m_vwiEad->wave()->raw.size();
		int nSeconds = nSamples / EAD_SAMPLES_PER_SECOND;
		int nMinutes = nSeconds / 60;
		if (nMinutes >= Globals->idacSettings()->nRecordingDuration)
			stopRecording(true, true);
	}

	// Tell the chart to update when in Recording mode
	emit updateRecordings();
}
