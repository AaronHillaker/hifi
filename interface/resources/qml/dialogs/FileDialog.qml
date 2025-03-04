import QtQuick 2.0
import QtQuick.Controls 1.4
import Qt.labs.folderlistmodel 2.1
import Qt.labs.settings 1.0

import ".."
import "../windows"
import "../styles"
import "../controls" as VrControls
import "fileDialog"

//FIXME implement shortcuts for favorite location
ModalWindow {
    id: root
    resizable: true
    width: 640
    height: 480

    Settings {
        category: "FileDialog"
        property alias width: root.width
        property alias height: root.height
        property alias x: root.x
        property alias y: root.y
    }


    // Set from OffscreenUi::getOpenFile()
    property alias caption: root.title;
    // Set from OffscreenUi::getOpenFile()
    property alias dir: model.folder;
    // Set from OffscreenUi::getOpenFile()
    property alias filter: selectionType.filtersString;
    // Set from OffscreenUi::getOpenFile()
    property int options; // <-- FIXME unused

    property bool selectDirectory: false;
    property bool showHidden: false;
    // FIXME implement
    property bool multiSelect: false;
    // FIXME implement
    property bool saveDialog: false;
    property var helper: fileDialogHelper
    property alias model: fileTableView.model

    signal selectedFile(var file);
    signal canceled();

    Rectangle {
        anchors.fill: parent
        color: "white"

        Row {
            id: navControls
            anchors { left: parent.left; top: parent.top; margins: 8 }
            spacing: 8
            // FIXME implement back button
            //VrControls.ButtonAwesome {
            //    id: backButton
            //    text: "\uf0a8"
            //    size: currentDirectory.height
            //    enabled: d.backStack.length != 0
            //    MouseArea { anchors.fill: parent; onClicked: d.navigateBack() }
            //}
            VrControls.ButtonAwesome {
                id: upButton
                enabled: model.parentFolder && model.parentFolder !== ""
                text: "\uf0aa"
                size: 32
                onClicked: d.navigateUp();
            }
            VrControls.ButtonAwesome {
                id: homeButton
                property var destination: helper.home();
                enabled: d.homeDestination ? true : false
                text: "\uf015"
                size: 32
                onClicked: d.navigateHome();
            }
        }

        TextField {
            id: currentDirectory
            height: homeButton.height
            anchors { left: navControls.right; right: parent.right; top: parent.top; margins: 8 }
            property var lastValidFolder: helper.urlToPath(model.folder)
            onLastValidFolderChanged: text = lastValidFolder;
            verticalAlignment: Text.AlignVCenter
            font.pointSize: 14
            font.bold: true

            // FIXME add support auto-completion
            onAccepted: {
                if (!helper.validFolder(text)) {
                    text = lastValidFolder;
                    return
                }
                model.folder = helper.pathToUrl(text);
            }
        }

        QtObject {
            id: d
            property var currentSelectionUrl;
            readonly property string currentSelectionPath: helper.urlToPath(currentSelectionUrl);
            property bool currentSelectionIsFolder;
            property var backStack: []
            property var tableViewConnection: Connections { target: fileTableView; onCurrentRowChanged: d.update(); }
            property var modelConnection: Connections { target: model; onFolderChanged: d.update(); }
            property var homeDestination: helper.home();
            Component.onCompleted: update();

            function update() {
                var row = fileTableView.currentRow;
                if (row === -1 && root.selectDirectory) {
                    currentSelectionUrl = fileTableView.model.folder;
                    currentSelectionIsFolder = true;
                    return;
                }

                currentSelectionUrl = fileTableView.model.get(row, "fileURL");
                currentSelectionIsFolder = fileTableView.model.isFolder(row);
                if (root.selectDirectory || !currentSelectionIsFolder) {
                    currentSelection.text = helper.urlToPath(currentSelectionUrl);
                }
            }

            function navigateUp() {
                if (model.parentFolder && model.parentFolder !== "") {
                    model.folder = model.parentFolder
                    return true;
                }
            }

            function navigateHome() {
                model.folder = homeDestination;
                return true;
            }
        }

        FileTableView {
            id: fileTableView
            anchors { left: parent.left; right: parent.right; top: currentDirectory.bottom; bottom: currentSelection.top; margins: 8 }
            onDoubleClicked: navigateToRow(row);
            focus: true
            Keys.onReturnPressed: navigateToCurrentRow();
            Keys.onEnterPressed: navigateToCurrentRow();
            model: FolderListModel {
                id: model
                nameFilters: selectionType.currentFilter
                showDirsFirst: true
                showDotAndDotDot: false
                showFiles: !root.selectDirectory
                // For some reason, declaring these bindings directly in the targets doesn't
                // work for setting the initial state
                Component.onCompleted: {
                    currentDirectory.lastValidFolder  = Qt.binding(function() { return helper.urlToPath(model.folder); });
                    upButton.enabled = Qt.binding(function() { return (model.parentFolder && model.parentFolder != "") ? true : false; });
                    showFiles = !root.selectDirectory
                }
                onFolderChanged: fileTableView.currentRow = 0;
            }

            function navigateToRow(row) {
                currentRow = row;
                navigateToCurrentRow();
            }

            function navigateToCurrentRow() {
                var row = fileTableView.currentRow
                var isFolder = model.isFolder(row);
                var file = model.get(row, "fileURL");
                if (isFolder) {
                    fileTableView.model.folder = file
                } else {
                    root.selectedFile(file);
                    root.destroy();
                }
            }
        }

        TextField {
            id: currentSelection
            anchors { right: root.selectDirectory ? parent.right : selectionType.left; rightMargin: 8; left: parent.left; leftMargin: 8; top: selectionType.top }
            readOnly: true
            activeFocusOnTab: false
        }

        FileTypeSelection {
            id: selectionType
            anchors { bottom: buttonRow.top; bottomMargin: 8; right: parent.right; rightMargin: 8; left: buttonRow.left }
            visible: !selectDirectory
            KeyNavigation.left: fileTableView
            KeyNavigation.right: openButton
        }

        Row {
            id: buttonRow
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            spacing: 8
            Button {
                id: openButton
                text: root.selectDirectory ? "Choose" : "Open"
                enabled: currentSelection.text ? true : false
                onClicked: { selectedFile(d.currentSelectionUrl); root.visible = false; }
                Keys.onReturnPressed: { selectedFile(d.currentSelectionUrl); root.visible = false; }

                KeyNavigation.up: selectionType
                KeyNavigation.left: selectionType
                KeyNavigation.right: cancelButton
            }
            Button {
                id: cancelButton
                text: "Cancel"
                KeyNavigation.up: selectionType
                KeyNavigation.left: openButton
                KeyNavigation.right: fileTableView.contentItem
                Keys.onReturnPressed: { canceled(); root.enabled = false }
                onClicked: { canceled(); root.visible = false; }
            }
        }
    }

    Keys.onPressed: {
        switch (event.key) {
        case Qt.Key_Backspace:
            event.accepted = d.navigateUp();
            break;

        case Qt.Key_Home:
            event.accepted = d.navigateHome();
            break;

        }
    }
}


