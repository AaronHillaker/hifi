//  Copyright 2016 High Fidelity, Inc.
//
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

(function() {

    var TARGET_OFFSET = {
        x: -1,
        y: 1,
        z: -1
    }

    var baseURL = "https://hifi-content.s3.amazonaws.com/DomainContent/CellScience/";

    var self = this;

    this.preload = function(entityId) {

        this.entityId = entityId;
        this.data = JSON.parse(Entities.getEntityProperties(this.entityId, "userData").userData);
        this.buttonImageURL = baseURL + "GUI/GUI_jump_off.png";
        this.addExitButton();
        this.isRiding = false;

        if (this.data && this.data.isDynein) {
            this.rotation = 180;
        } else {
            this.rotation = 0;
        }

    }

    this.addExitButton = function() {
        this.windowDimensions = Controller.getViewportDimensions();
        this.buttonWidth = 75;
        this.buttonHeight = 75;
        this.buttonPadding = 10;

        this.buttonPositionX = (self.windowDimensions.x - self.buttonPadding) / 2 - self.buttonWidth / 2;
        this.buttonPositionY = (self.windowDimensions.y - self.buttonHeight) - (self.buttonHeight + self.buttonPadding);
        this.exitButton = Overlays.addOverlay("image", {
            x: self.buttonPositionX,
            y: self.buttonPositionY,
            width: self.buttonWidth,
            height: self.buttonHeight,
            imageURL: self.buttonImageURL,
            visible: false,
            alpha: 1.0
        });
    }

    this.clickReleaseOnEntity = function(entityId, mouseEvent) {
        // print('CLICKED ON MOTOR PROTEIN')
        if (mouseEvent.isLeftButton && !self.isRiding) {
            print("GET ON");
            self.isRiding = true;
            if (!self.entityId) {
                self.entityId = entityId;
            }
            self.entityLocation = Entities.getEntityProperties(this.entityId, "position").position;
            self.targetLocation = Vec3.sum(self.entityLocation, TARGET_OFFSET);
            Overlays.editOverlay(self.exitButton, {
                visible: true
            });
            Controller.mousePressEvent.connect(this.onMousePress);
            Script.update.connect(this.update);
        }
    }

    this.lastAvatarPosition = null;
    this.lastEntityPosition = null;
    this.update = function(deltaTime) {
        if (self.isRiding !== true) {
            return
        }

        Entities.editEntity(self.entityId, {
            velocity: {
                x: 1,
                y: 0,
                z: 0
            }
        })
        self.lastEntityLocation = self.entityLocation;
        self.lastTargetLocation = self.targetLocation
        self.entityLocation = Entities.getEntityProperties(self.entityId, "position").position;
        self.targetLocation = Vec3.sum(self.entityLocation, TARGET_OFFSET);
        //  print('Script.clearTimeout self.lastTargetLocation' + JSON.stringify(self.lastTargetLocation))
        //  print('Script.clearTimeout self.targetLocation' + JSON.stringify(self.targetLocation))
        var diff = Vec3.distance(self.targetLocation, self.lastTargetLocation);
        // print('Script.clearTimeout diff is::' + diff)
        self.addThrustToAvatar(deltaTime);
    }


    this.addThrustToAvatar = function(deltaTime) {
        var targetCurrentLocationToLastLocation = Vec3.subtract(self.targetLocation, self.lastTargetLocation);

        // print('Script.clearTimeout targetCurrentLocationToLastLocation' + JSON.stringify(targetCurrentLocationToLastLocation));
        // print('Script.clearTimeout deltaTime' + deltaTime)
        // print('Script.clearTimeout velocity' + JSON.stringify(self.velocity))
        var thrustToAdd = Vec3.multiply(100, targetCurrentLocationToLastLocation);
        thrustToAdd = Vec3.multiply(thrustToAdd, 1 / deltaTime);
        // print('Script.clearTimeout adding thrust!' + JSON.stringify(thrustToAdd))

        MyAvatar.addThrust(thrustToAdd);

    }

    this.onMousePress = function(event) {
        var clickedOverlay = Overlays.getOverlayAtPoint({
            x: event.x,
            y: event.y
        });
        if (event.isLeftButton && clickedOverlay === self.exitButton) {
            print("GET OFF");
            Script.update.disconnect(this.update);
            self.reset();
        }
    }

    this.reset = function() {
        // print('reset')
        if (self.isRiding) {
            Overlays.editOverlay(this.exitButton, {
                visible: false
            });
        }
        self.isRiding = false;
    }

    this.unload = function() {
        // print("unload");
        self.reset();

        Controller.mousePressEvent.disconnect(this.onMousePress);
    }


});