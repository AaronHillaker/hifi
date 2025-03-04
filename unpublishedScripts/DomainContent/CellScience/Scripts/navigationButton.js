
//  Copyright 2016 High Fidelity, Inc.
//
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() {

    var version = 1;
    var added = false;
    this.frame = 0;
    var utilsScript = Script.resolvePath('utils.js');
    Script.include(utilsScript);

    var self = this;
    var baseURL = "https://hifi-content.s3.amazonaws.com/DomainContent/CellScience/";

    this.preload = function(entityId) {
        this.entityId = entityId;
        var mySavedSettings = Settings.getValue(entityId);

        if (mySavedSettings.buttons !== undefined) {
            // print('NAV preload buttons'+ mySavedSettings.buttons)
            mySavedSettings.buttons.forEach(function(b) {
                // print('NAV deleting button'+ b)
                Overlays.deleteOverlay(b);
            })
            Settings.setValue(entityId,'')
        }


        self.getUserData();
        this.buttonImageURL = baseURL + "GUI/GUI_" + self.userData.name + ".png?" + version;
        if (self.button === undefined) {
            // print('NAV NO BUTTON ADDING ONE!!')
            self.button = true;
            self.addButton();

        } else {
            // print('NAV SELF ALREADY HAS A BUTTON!!')
        }

    }

    this.addButton = function() {


        self.getUserData();
        this.windowDimensions = Controller.getViewportDimensions();
        this.buttonWidth = 150;
        this.buttonHeight = 50;
        this.buttonPadding = 10;

        this.buttonPositionX = (self.userData.offset + 1) * (this.buttonWidth + this.buttonPadding) + (self.windowDimensions.x / 2) - (this.buttonWidth * 3 + this.buttonPadding * 2.5);
        this.buttonPositionY = (self.windowDimensions.y - self.buttonHeight) - 50;
        this.button = Overlays.addOverlay("image", {
            x: self.buttonPositionX,
            y: self.buttonPositionY,
            width: self.buttonWidth,
            height: self.buttonHeight,
            imageURL: self.buttonImageURL,
            visible: true,
            alpha: 1.0
        });

        var mySavedSettings = Settings.getValue(this.entityId);
        var buttons = [];
        if (mySavedSettings.buttons !== undefined) {
            buttons = mySavedSettings.buttons;
            buttons.push(this.button);
        } else {
            buttons.push(this.button);
        }
        // print('NAV ENTITY ID IN ADDBUTTON'+ this.entityId)
        // print('NAV BUTTONS IN ADDBUTTON:: '+ buttons)
        Settings.setValue(this.entityId, {
            buttons: buttons
        });

    }



    this.update = function(deltaTime) {
        if (self.frame < 10) {
            self.frame++;
        } else {
            //          this.lookAt(this.userData.target);
        }
    }

    this.onClick = function(event) {
        var clickedOverlay = Overlays.getOverlayAtPoint({
            x: event.x,
            y: event.y
        });


        if (clickedOverlay == self.button) {
            // print("NAV Clicked navigation button: " + self.userData.name + ", and looking at " + self.userData.target.x + ", " + self.userData.target.y + ", " + self.userData.target.z);

            self.lookAtTarget();
        }

    }

    this.lookAtTarget = function() {
        self.getUserData();
        var direction = Vec3.normalize(Vec3.subtract(self.userData.entryPoint, self.userData.target));
        var pitch = Quat.angleAxis(Math.asin(-direction.y) * 180.0 / Math.PI, {
            x: 1,
            y: 0,
            z: 0
        });
        var yaw = Quat.angleAxis(Math.atan2(direction.x, direction.z) * 180.0 / Math.PI, {
            x: 0,
            y: 1,
            z: 0
        });

        MyAvatar.goToLocation(self.userData.entryPoint, true, yaw);

        MyAvatar.headYaw = 0;

    }

    this.getUserData = function() {
        this.properties = Entities.getEntityProperties(this.entityId);
        if (self.properties.userData) {
            this.userData = JSON.parse(this.properties.userData);
        } else {
            this.userData = {};
        }
    }

    var buttonDeleter;
    var deleterCount = 0;
    this.unload = function() {
        // print('NAV UNLOAD - BUTTON, ENTITY -- ' + this.button + " // " + this.entityId)

        Overlays.deleteOverlay(self.button);

        Controller.mousePressEvent.disconnect(this.onClick);
        // Script.update.disconnect(this.update);
    }

    Controller.mousePressEvent.connect(this.onClick);
    // Script.update.connect(this.update);

});