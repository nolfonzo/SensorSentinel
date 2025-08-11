<template>
  <div style="width: 100%">
    <style>
      /* Set general font size for all elements within the container */
      .v-container {
        font-size: 0.75rem;
      }

      .v-card-title {
        font-size: 0.85rem;
      }

      .v-btn {
        font-size: 0.7rem;
        padding: 4px 8px;
      }

      .v-text-field input {
        font-size: 0.75rem;
        padding: 4px;
      }

      .v-data-table {
        font-size: 0.75rem;
      }

      .nr-dashboard-cardpanel,
      .nr-dashboard-template {
        width: 100% !important;
      }
    </style>

    <v-container fluid class="pa-6" style="max-width: 100%; padding: 0">
      <!-- Main Tabs -->
      <v-tabs v-model="activeSection" class="mb-6">
        <v-tab value="owners">
          <v-icon class="mr-2">mdi-account-group</v-icon>
          Owner Management
        </v-tab>
        <v-tab value="devices">
          <v-icon class="mr-2">mdi-router-network</v-icon>
          Device Management
        </v-tab>
      </v-tabs>

      <v-window v-model="activeSection">
        <!-- Owner Management Tab -->
        <v-window-item value="owners">
          <div class="mb-4">
            <h2 class="text-h4 mb-6 d-flex align-center">
              <v-icon class="mr-2" color="primary">mdi-account-group</v-icon>
              Owner Management
            </h2>

            <!-- Add New Owner Card -->
            <v-card class="mb-6">
              <v-card-title>
                <v-icon class="mr-2">mdi-account-plus</v-icon>
                Add New Owner
              </v-card-title>
              <v-card-text>
                <v-row>
                  <v-col cols="12" md="4" lg="3">
                    <v-text-field v-model="newOwner.name" label="Owner Name" variant="outlined" density="compact"
                      :error="!newOwner.name.trim() && attempted"
                      :error-messages="!newOwner.name.trim() && attempted ? 'Name is required' : ''">
                    </v-text-field>
                  </v-col>
                  <v-col cols="12" md="4" lg="3">
                    <v-text-field v-model="newOwner.email" label="Email" variant="outlined" density="compact"
                      type="email"></v-text-field>
                  </v-col>
                  <v-col cols="12" md="4" lg="3">
                    <v-text-field v-model="newOwner.phone" label="Phone" variant="outlined" density="compact">
                    </v-text-field>
                  </v-col>
                  <v-col cols="12" lg="3" class="d-flex align-center">
                    <v-btn @click="addOwner" color="primary" :disabled="!newOwner.name.trim()" prepend-icon="mdi-plus"
                      size="large" block>
                      Add Owner
                    </v-btn>
                  </v-col>
                </v-row>
              </v-card-text>
            </v-card>

            <!-- Owners List -->
            <v-card>
              <v-card-title class="d-flex align-center">
                <v-icon class="mr-2">mdi-account-group</v-icon>
                Property Owners ({{ filteredOwnersList.length }} of {{
                Object.keys(owners).length }})
                <v-spacer></v-spacer>
                <v-text-field v-model="ownerSearchFilter" label="Search owners..." prepend-inner-icon="mdi-magnify"
                  variant="outlined" density="compact" hide-details clearable style="max-width: 400px" class="ml-4">
                </v-text-field>
              </v-card-title>

              <v-data-table :headers="ownerHeaders" :items="filteredOwnersList" class="elevation-0" density="compact"
                :items-per-page="20">
                <template v-slot:item.deviceCount="{ item }">
                  <v-chip size="small" :color="item.deviceCount > 0 ? 'success' : 'default'">
                    {{ item.deviceCount }}
                  </v-chip>
                </template>

                <template v-slot:item.actions="{ item }">
                  <v-btn icon="mdi-pencil" size="small" variant="text" @click="editOwner(item)">
                  </v-btn>
                  <v-btn icon="mdi-delete" size="small" variant="text" color="error" @click="deleteOwner(item.name)">
                  </v-btn>
                </template>
              </v-data-table>
            </v-card>
          </div>
        </v-window-item>

        <!-- Device Management Tab -->
        <v-window-item value="devices">
          <div class="mb-4">
            <h2 class="text-h4 mb-6 d-flex align-center">
              <v-icon class="mr-2" color="success">mdi-router-network</v-icon>
              Device Management
            </h2>

            <!-- Add New Device Card -->
            <v-card class="mb-6">
              <v-card-title>
                <v-icon class="mr-2">mdi-chip</v-icon>
                Add New Device
              </v-card-title>
              <v-card-text>
                <v-row class="mb-4">
                  <v-col cols="12" md="3" lg="2">
                    <v-text-field v-model="newDevice.nodeId" label="Node ID" variant="outlined" density="compact"
                      hint="Unique device identifier" persistent-hint :error="!!errorMessage"
                      :error-messages="errorMessage" @input="clearError"></v-text-field>
                  </v-col>
                  <v-col cols="12" md="3" lg="2">
                    <v-text-field v-model="newDevice.displayName" label="Display Name" variant="outlined"
                      density="compact" hint="User-friendly name" persistent-hint></v-text-field>
                  </v-col>
                  <v-col cols="12" md="3" lg="3">
                    <v-autocomplete v-model="newDevice.ownerName" :items="ownerOptions" label="Owner" variant="outlined"
                      density="compact" clearable :filter-keys="['title']">
                    </v-autocomplete>
                  </v-col>
                </v-row>

                <!-- Add Device Button -->
                <v-row class="mb-4">
                  <v-col cols="12" lg="4" class="d-flex align-center">
                    <v-btn @click="addDevice" color="primary" :disabled="!canAddDevice" prepend-icon="mdi-plus"
                      size="large" block>
                      Add Device
                    </v-btn>
                  </v-col>
                </v-row>
              </v-card-text>
            </v-card>

            <!-- Device Filter Controls -->
            <v-card class="mb-6">
              <v-card-title>Filter & Search Devices</v-card-title>
              <v-card-text>
                <v-row>
                  <v-col cols="12" md="6" lg="4">
                    <v-text-field v-model="deviceSearchQuery" label="Search Devices" prepend-inner-icon="mdi-magnify"
                      variant="outlined" density="compact" clearable hint="Search by Node ID or Display Name">
                    </v-text-field>
                  </v-col>
                  <v-col cols="12" md="6" lg="4">
                    <v-autocomplete v-model="selectedOwnerFilter" :items="ownerFilterOptions" label="Filter by Owner"
                      prepend-inner-icon="mdi-account-filter" variant="outlined" density="compact" clearable
                      :filter-keys="['title']">
                    </v-autocomplete>
                  </v-col>
                  <v-col cols="12" lg="4" class="d-flex align-center">
                    <v-chip class="mr-2" color="info">
                      Total: {{ Object.keys(devices).length }}
                    </v-chip>
                    <v-chip color="success">
                      Filtered: {{ filteredDevicesList.length }}
                    </v-chip>
                  </v-col>
                </v-row>
              </v-card-text>
            </v-card>

            <!-- Devices Data Table -->
            <v-card>
              <v-card-title>
                <v-icon class="mr-2">mdi-router-network</v-icon>
                Registered Devices ({{ filteredDevicesList.length }} of {{
                Object.keys(devices).length }})
              </v-card-title>

              <v-data-table :headers="deviceHeaders" :items="filteredDevicesList" class="elevation-0" density="compact"
                :items-per-page="20">
                <!-- Display Node ID in the Devices Table -->
                <template v-slot:item.nodeId="{ item }">
                  <v-chip size="small" color="secondary">{{ item.nodeId }}</v-chip>
                </template>

                <!-- Display Display Name in the Devices Table -->
                <template v-slot:item.displayName="{ item }">
                  <v-chip size="small" color="info">{{ item.displayName }}</v-chip>
                </template>

                <!-- Display Owner in the Devices Table -->
                <template v-slot:item.ownerName="{ item }">
                  <v-chip size="small" color="primary">{{ item.ownerName }}</v-chip>
                </template>

                <template v-slot:item.pins="{ item }">
                  <div>
                    <v-chip size="small" color="success" class="mr-1">
                      B: {{ getBinaryPinCount(item) }}/8
                    </v-chip>
                    <v-chip size="small" color="info">
                      A: {{ getAnalogPinCount(item) }}/4
                    </v-chip>
                  </div>
                </template>

                <template v-slot:item.lastSeen="{ item }">
                  <span :class="item.lastSeen ? 'text-success' : 'text-error'">
                    {{ item.lastSeen ? formatDate(item.lastSeen) : 'Never' }}
                  </span>
                </template>

                <template v-slot:item.actions="{ item }">
                  <v-btn icon="mdi-eye" size="small" variant="text" @click="viewDevice(item)"></v-btn>
                  <v-btn icon="mdi-pencil" size="small" variant="text" @click="editDevice(item)"></v-btn>
                  <v-btn icon="mdi-delete" size="small" variant="text" color="error" @click="deleteDevice(item.nodeId)">
                  </v-btn>
                </template>
              </v-data-table>
            </v-card>
          </div>
        </v-window-item>
      </v-window>
    </v-container>

    <!-- Dialogs below this line -->
    <!-- Edit Owner Dialog -->
    <v-dialog v-model="editOwnerDialog" max-width="600px">
      <v-card>
        <v-card-title>Edit Owner</v-card-title>
        <v-card-text>
          <v-text-field v-model="editingOwner.name" label="Name" variant="outlined"></v-text-field>
          <v-text-field v-model="editingOwner.email" label="Email" variant="outlined"></v-text-field>
          <v-text-field v-model="editingOwner.phone" label="Phone" variant="outlined"></v-text-field>
        </v-card-text>
        <v-card-actions>
          <v-spacer></v-spacer>
          <v-btn @click="editOwnerDialog = false">Cancel</v-btn>
          <v-btn @click="saveOwner" color="primary">Save</v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <!-- View Device Dialog -->
    <v-dialog v-model="viewDialog" max-width="1000px">
      <v-card v-if="selectedDevice">
        <v-card-title>
          <v-icon class="mr-2">mdi-eye</v-icon>
          Device Details - {{ selectedDevice.displayName }}
        </v-card-title>
        <v-card-text>
          <v-row>
            <v-col cols="12" md="6" lg="3">
              <v-text-field :model-value="selectedDevice.nodeId" label="Node ID" variant="outlined" readonly>
              </v-text-field>
            </v-col>
            <v-col cols="12" md="6" lg="3">
              <v-text-field :model-value="selectedDevice.displayName" label="Display Name" variant="outlined" readonly>
              </v-text-field>
            </v-col>
            <v-col cols="12" md="6" lg="3">
              <v-text-field :model-value="selectedDevice.ownerName" label="Owner" variant="outlined" readonly>
              </v-text-field>
            </v-col>
            <v-col cols="12" md="6" lg="3">
              <v-text-field :model-value="selectedDevice.lastSeen ? formatDate(selectedDevice.lastSeen) : 'Never'"
                label="Last Seen" variant="outlined" readonly></v-text-field>
            </v-col>
          </v-row>

          <v-card variant="outlined" class="mt-4">
            <v-card-title>Pin Configuration</v-card-title>
            <v-card-text>
              <v-tabs v-model="viewTab">
                <v-tab value="binary">Binary Pins</v-tab>
                <v-tab value="analog">Analog Pins</v-tab>
              </v-tabs>

              <v-window v-model="viewTab" class="mt-4">
                <v-window-item value="binary">
                  <v-row>
                    <v-col v-for="(pin, index) in selectedDevice.binaryPins" :key="'view-bin-' + index" cols="12" sm="6"
                      md="3" lg="2" xl="1-5">
                      <v-text-field :model-value="pin ? pin.label : ''" :label="`Binary Pin ${index}`"
                        variant="outlined" density="compact" readonly></v-text-field>
                    </v-col>
                  </v-row>
                </v-window-item>

                <v-window-item value="analog">
                  <v-row>
                    <v-col v-for="(pin, index) in selectedDevice.analogPins" :key="'view-ana-' + index" cols="12" sm="6"
                      md="3" lg="2">
                      <v-text-field :model-value="pin ? pin.value : ''" :label="`Analog Pin ${index}`"
                        variant="outlined" density="compact" readonly></v-text-field>
                    </v-col>
                  </v-row>
                </v-window-item>
              </v-window>
            </v-card-text>
          </v-card>
        </v-card-text>
        <v-card-actions>
          <v-spacer></v-spacer>
          <v-btn @click="viewDialog = false">Close</v-btn>
          <v-btn @click="editDevice(selectedDevice)" color="primary">Edit</v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <!-- Edit Device Dialog -->
    <v-dialog v-model="editDialog" max-width="1000px">
      <v-card v-if="editingDevice">
        <v-card-title>
          <v-icon class="mr-2">mdi-pencil</v-icon>
          Edit Device - {{ editingDevice.displayName }}
        </v-card-title>
        <v-card-text>
          <v-row>
            <v-col cols="12" md="6" lg="3">
              <v-text-field v-model="editingDevice.nodeId" label="Node ID" variant="outlined" readonly
                hint="Node ID cannot be changed" persistent-hint>
              </v-text-field>
            </v-col>
            <v-col cols="12" md="6" lg="3">
              <v-text-field v-model="editingDevice.displayName" label="Display Name" variant="outlined">
              </v-text-field>
            </v-col>
            <v-col cols="12" lg="6">
              <v-autocomplete v-model="editingDevice.ownerName" :items="ownerOptions" label="Owner" variant="outlined"
                clearable :filter-keys="['title']">
              </v-autocomplete>
            </v-col>
          </v-row>

          <v-card variant="outlined" class="mt-4">
            <v-card-title>Pin Configuration</v-card-title>
            <v-card-text>
              <v-tabs v-model="editTab">
                <v-tab value="binary">Binary Pins</v-tab>
                <v-tab value="analog">Analog Pins</v-tab>
              </v-tabs>

              <v-window v-model="editTab" class="mt-4">
                <v-window-item value="binary">
                  <v-row>
                    <v-col v-for="(pin, index) in editingDevice.binaryPins" :key="'edit-bin-' + index" cols="12" sm="6"
                      md="3" lg="2" xl="1-5">
                      <v-card class="pa-2 mb-2" outlined>
                        <v-card-text>
                          <v-text-field v-model="editingDevice.binaryPins[index].label" :label="`Binary Pin ${index}`"
                            variant="outlined" density="compact" :placeholder="`Pin ${index} Label`"></v-text-field>

                          <v-select v-model="editingDevice.binaryPins[index].trigger" :items="['High', 'Low', 'Change']"
                            label="Trigger" variant="outlined" density="compact">
                          </v-select>

                          <v-select v-model="editingDevice.binaryPins[index].alertLevel"
                            :items="['None', 'Low', 'Medium', 'High']" label="Alert Level" variant="outlined"
                            density="compact">
                          </v-select>
                        </v-card-text>
                      </v-card>
                    </v-col>
                  </v-row>
                </v-window-item>

                <v-window-item value="analog">
                  <v-row>
                    <v-col v-for="(pin, index) in editingDevice.analogPins" :key="'edit-ana-' + index" cols="12" sm="6"
                      md="3" lg="2" xl="1-5">
                      <v-card class="pa-2 mb-2" outlined>
                        <v-card-text>
                          <v-text-field v-model="editingDevice.analogPins[index].value" :label="`Analog Pin ${index}`"
                            variant="outlined" density="compact" :placeholder="`Pin A${index} Label`"></v-text-field>

                          <v-text-field v-model="editingDevice.analogPins[index].lowThreshold" label="Low Threshold"
                            variant="outlined" density="compact" type="number" placeholder="Low Threshold">
                          </v-text-field>

                          <v-text-field v-model="editingDevice.analogPins[index].highThreshold" label="High Threshold"
                            variant="outlined" density="compact" type="number" placeholder="High Threshold">
                          </v-text-field>

                          <v-select v-model="editingDevice.analogPins[index].alertLevel"
                            :items="['None', 'Low', 'Medium', 'High']" label="Alert Level" variant="outlined"
                            density="compact">
                          </v-select>
                        </v-card-text>
                      </v-card>
                    </v-col>
                  </v-row>
                </v-window-item>
              </v-window>
            </v-card-text>
          </v-card>
        </v-card-text>

        <v-card-actions>
          <v-spacer></v-spacer>
          <v-btn @click="editDialog = false">Cancel</v-btn>
          <v-btn @click="saveDevice" color="primary">Save</v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <!-- Owner Devices Dialog -->
    <v-dialog v-model="ownerDevicesDialog" max-width="1200px">
      <v-card v-if="selectedOwner">
        <v-card-title>
          <v-icon class="mr-2">mdi-account-circle</v-icon>
          Devices for {{ selectedOwner.name }}
        </v-card-title>
        <v-card-text>
          <v-data-table :headers="deviceHeaders.slice(0, -1)" :items="ownerDevices" class="elevation-0"
            density="compact" :items-per-page="15">
            <template v-slot:item.nodeId="{ item }">
              <v-chip size="small" color="secondary">{{ item.nodeId }}</v-chip>
            </template>

            <template v-slot:item.displayName="{ item }">
              <v-chip size="small" color="info">{{ item.displayName }}</v-chip>
            </template>

            <template v-slot:item.pins="{ item }">
              <div>
                <v-chip size="small" color="success" class="mr-1">
                  B: {{ getBinaryPinCount(item) }}/8
                </v-chip>
                <v-chip size="small" color="info">
                  A: {{ getAnalogPinCount(item) }}/4
                </v-chip>
              </div>
            </template>

            <template v-slot:item.lastSeen="{ item }">
              <span :class="item.lastSeen ? 'text-success' : 'text-error'">
                {{ item.lastSeen ? formatDate(item.lastSeen) : 'Never' }}
              </span>
            </template>
          </v-data-table>
        </v-card-text>
        <v-card-actions>
          <v-spacer></v-spacer>
          <v-btn @click="ownerDevicesDialog = false">Close</v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>
  </div>
</template>

<script>
  export default {
    data() {
      console.log("**** STARTING");
      return {
        activeSection: "owners", // Default to owners tab

        // Owner data
        owners: {},
        attempted: false,
        ownerSearchFilter: "",
        newOwner: {
          name: "", // Unique identifier and display name
          email: "",
          phone: "",
        },
        editOwnerDialog: false,
        editingOwner: {},
        ownerHeaders: [
          { title: "Name", key: "name" }, // Unique name for owners
          { title: "Email", key: "email" },
          { title: "Phone", key: "phone" },
          { title: "Devices", key: "deviceCount" }, // Change to Devices
          { title: "Actions", key: "actions", sortable: false },
        ],

        // Device data
        devices: {}, // Data structure for devices
        errorMessage: "",
        deviceSearchQuery: "", // Search query for devices
        selectedOwnerFilter: null,
        activeTab: "binary",
        viewTab: "binary",
        editTab: "binary",
        viewDialog: false,
        editDialog: false,
        ownerDevicesDialog: false, // Dialog to show devices owned by the selected owner
        selectedDevice: null, // Device selected for viewing
        editingDevice: null, // Device selected for editing
        selectedOwner: null,
        newDevice: {
          // New device input fields
          nodeId: "",           // Unique device identifier (from MAC)
          displayName: "",      // User-friendly name
          ownerName: "",
          binaryPins: Array(8).fill(""),
          analogPins: Array(4).fill(""),
        },
        deviceHeaders: [
          // Table headers for devices
          { title: "Node ID", key: "nodeId", width: "120px" },
          { title: "Display Name", key: "displayName" },
          { title: "Owner", key: "ownerName" },
          { title: "Pins", key: "pins", width: "140px" },
          { title: "Last Seen", key: "lastSeen" },
          { title: "Actions", key: "actions", sortable: false },
        ],
      };
    },

    computed: {
      // Owner computeds
      ownersList() {
        return Object.entries(this.owners).map(([name, owner]) => ({
          name, // Use name as key and display name
          ...owner,
          deviceCount: owner.devices ? owner.devices.length : 0, // Count devices owned
        }));
      },

      filteredOwnersList() {
        if (!this.ownerSearchFilter) return this.ownersList;

        const filter = this.ownerSearchFilter.toLowerCase();
        return this.ownersList.filter(
          (owner) =>
            owner.name.toLowerCase().includes(filter) ||
            (owner.email && owner.email.toLowerCase().includes(filter)) ||
            (owner.phone && owner.phone.includes(filter))
        );
      },

      // Device computeds
      ownerOptions() {
        console.log(
          "********* owners data: " + JSON.stringify(this.owners, null, 2)
        );

        // Map over the owners to create dropdown options
        const options = Object.entries(this.owners).map(
          ([ownerKey, owner]) => ({
            title: owner.name, // Use the name property for display
            value: ownerKey, // Use the unique key as the identifier
          })
        );

        console.log("********* Owner options: ", options);
        return options;
      },

      ownerFilterOptions() {
        return [{ title: "All Owners", value: null }, ...this.ownerOptions];
      },

      devicesList() {
        // Returns list of devices
        return Object.values(this.devices);
      },

      filteredDevicesList() {
        // Filters the device list based on search query
        let filtered = this.devicesList;

        if (this.deviceSearchQuery && this.deviceSearchQuery.trim()) {
          const query = this.deviceSearchQuery.trim().toLowerCase();
          filtered = filtered.filter((device) => {
            const nodeId = (device.nodeId || "").toLowerCase();
            const displayName = (device.displayName || "").toLowerCase();
            return nodeId.includes(query) || displayName.includes(query);
          });
        }

        if (this.selectedOwnerFilter) {
          filtered = filtered.filter(
            (device) => device.ownerName === this.selectedOwnerFilter
          );
        }

        return filtered;
      },

      ownerDevices() {
        // Returns devices owned by the selected owner
        if (!this.selectedOwner) return [];
        return this.devicesList.filter(
          (device) => device.ownerName === this.selectedOwner.name
        );
      },

      canAddDevice() {
        // Checks if we can add a new device
        return this.newDevice.nodeId && this.newDevice.nodeId.trim() && 
               this.newDevice.displayName && this.newDevice.displayName.trim();
      },
    },

    methods: {
      // Owner methods
      addOwner() {
        this.attempted = true;

        if (!this.newOwner.name.trim()) return; // Ensure name is filled to add owner

        // Send request to add new owner
        this.send({
          topic: "add_owner",
          payload: {
            name: this.newOwner.name.trim(),
            email: this.newOwner.email.trim(),
            phone: this.newOwner.phone.trim(),
          },
        });

        // Reset new owner fields
        this.newOwner = { name: "", email: "", phone: "" };
        this.attempted = false;
      },

      editOwner(owner) {
        this.editingOwner = { ...owner }; // Copy selected owner for editing
        this.editOwnerDialog = true;
      },

      saveOwner() {
        this.send({
          topic: "update_owner",
          payload: {
            name: this.editingOwner.name, // Ensure we send the name
            email: this.editingOwner.email,
            phone: this.editingOwner.phone,
            devices: this.owners[this.editingOwner.name].devices || [], // Using name as key
          },
        });
        this.editOwnerDialog = false;
      },

      deleteOwner(ownerName) {
        // Use name for deletion
        const owner = this.owners[ownerName]; // Find the owner by name
        const deviceCount = owner.devices ? owner.devices.length : 0;

        const message =
          deviceCount > 0
            ? `Owner "${owner.name}" has ${deviceCount} devices. Delete anyway?` // Change to devices
            : `Delete owner "${owner.name}"?`;

        if (confirm(message)) {
          this.send({
            topic: "delete_owner",
            payload: { name: ownerName }, // Pass the name to delete
          });
        }
      },

      // Device methods
      addDevice() {
        // Adds a new device
        if (!this.canAddDevice) return; // Check if device can be added

        this.errorMessage = "";

        // Construct the binary pins with the new structure
        const binaryPins = Array(8).fill(null).map((_, index) => ({
          label: this.newDevice.binaryPins[index] ? this.newDevice.binaryPins[index].label || "" : "",
          trigger: this.newDevice.binaryPins[index] ? this.newDevice.binaryPins[index].trigger || "High" : "High",
          alertLevel: this.newDevice.binaryPins[index] ? this.newDevice.binaryPins[index].alertLevel || "None" : "None"
        }));

        // Construct the analog pins with the new structure
        const analogPins = Array(4).fill(null).map((_, index) => ({
          value: this.newDevice.analogPins[index] ? this.newDevice.analogPins[index].value || "" : "",
          lowThreshold: this.newDevice.analogPins[index] ? this.newDevice.analogPins[index].lowThreshold || 0 : 0,
          highThreshold: this.newDevice.analogPins[index] ? this.newDevice.analogPins[index].highThreshold || 100 : 100,
          alertLevel: this.newDevice.analogPins[index] ? this.newDevice.analogPins[index].alertLevel || "None" : "None"
        }));

        // Send the new device to be added
        this.send({
          topic: "add_device",
          payload: {
            nodeId: this.newDevice.nodeId.trim(),
            displayName: this.newDevice.displayName.trim(),
            ownerName: this.newDevice.ownerName,
            binaryPins: binaryPins,
            analogPins: analogPins,
          },
        });

        // Reset new device fields
        this.newDevice = {
          nodeId: "",
          displayName: "",
          ownerName: "",
          binaryPins: Array(8).fill(""),
          analogPins: Array(4).fill(""),
        };
      },

      editDevice(device) {
        console.log("Editing device:", device);

        if (!device) {
          console.error("No device found to edit");
          return;
        }

        this.editingDevice = {
          nodeId: device.nodeId,        // Keep nodeId immutable
          displayName: device.displayName, // Allow displayName changes
          ownerName: device.ownerName,
          binaryPins:
            device.binaryPins && device.binaryPins.length > 0
              ? device.binaryPins.map((pin) => ({
                  label: pin.label || "",
                  trigger: pin.trigger || "High",
                  alertLevel: pin.alertLevel || "None",
                }))
              : Array(8).fill({
                  label: "",
                  trigger: "High",
                  alertLevel: "None",
                }),
          analogPins:
            device.analogPins && device.analogPins.length > 0
              ? device.analogPins.map((pin) => ({
                  value: pin.value || "",
                  lowThreshold: pin.lowThreshold || 0,
                  highThreshold: pin.highThreshold || 100,
                  alertLevel: pin.alertLevel || "None",
                }))
              : Array(4).fill({
                  value: "",
                  lowThreshold: 0,
                  highThreshold: 100,
                  alertLevel: "None",
                }),
          lastSeen: device.lastSeen,
        };

        this.editDialog = true;
        this.editTab = "binary";
        this.viewDialog = false;

        console.log("Current editingDevice state:", this.editingDevice);
      },

      saveDevice() {
        // Save updated device information
        this.send({
          topic: "update_device",
          payload: {
            nodeId: this.editingDevice.nodeId,  // Use nodeId as immutable key
            displayName: this.editingDevice.displayName,
            ownerName: this.editingDevice.ownerName,
            binaryPins: this.editingDevice.binaryPins,
            analogPins: this.editingDevice.analogPins,
            lastSeen: this.editingDevice.lastSeen,
          },
        });
        this.editDialog = false;
      },

      deleteDevice(nodeId) {
        // Delete a device using its nodeId
        console.log("Trying to delete device:", nodeId);
        if (confirm(`Delete device "${nodeId}"?`)) {
          this.send({
            topic: "delete_device",
            payload: { nodeId: nodeId }, // Pass the nodeId to delete
          });
        }
      },

      viewDevice(device) {
        this.selectedDevice = device;
        this.viewDialog = true;
        this.viewTab = "binary";
      },

      showOwnerDevices(ownerName) {
        // Show devices owned by the selected owner
        this.selectedOwner = {
          name: ownerName,
          ...this.owners[ownerName],
        };
        this.ownerDevicesDialog = true;
      },

      // Utility methods
      formatDate(dateStr) {
        // Format dates for display
        return new Date(dateStr).toLocaleString();
      },

      clearError() {
        this.errorMessage = ""; // Clear any error messages
      },

      getBinaryPinCount(item) {
        // Count non-empty binary pins
        if (!item.binaryPins) return 0;
        return item.binaryPins.filter(
          (pin) => pin && pin.label && pin.label.trim()
        ).length; // Ensure pin.label exists
      },

      getAnalogPinCount(item) {
        // Count non-empty analog pins
        if (!item.analogPins) return 0;
        return item.analogPins.filter(
          (pin) => pin && typeof pin.value === "string" && pin.value.trim()
        ).length; // Ensure pin.value is a string
      },
    },

    mounted() {
      this.send({ topic: "get_owners" }); // Request owners from the server
      this.send({ topic: "get_devices" }); // Request devices from the server
    },

    watch: {
      msg: {
        handler(newMsg) {
          console.log(
            "*** watch received topic: " +
              newMsg.topic +
              " payload: " +
              JSON.stringify(newMsg.payload)
          );

          if (newMsg && newMsg.topic === "owners_data") {
            this.owners = newMsg.payload || {}; // Set owners data received from the server
          } else if (newMsg && newMsg.topic === "devices_data") {
            // Handle devices data
            this.devices = newMsg.payload || {}; // Set devices data
          } else if (newMsg && newMsg.topic === "device_error") {
            // Handle device-related errors
            this.errorMessage = newMsg.payload.error;
          } else if (newMsg && newMsg.topic === "device_added") {
            // Device added successfully - use nodeId as key
            this.devices = {
              ...this.devices,
              [newMsg.payload.nodeId]: newMsg.payload,
            };
            this.errorMessage = "";
            console.log("Device added:", newMsg.payload.nodeId);
          } else if (newMsg && newMsg.topic === "device_updated") {
            // Device updated successfully - use nodeId as key
            this.devices = {
              ...this.devices,
              [newMsg.payload.nodeId]: newMsg.payload,
            };
            console.log("Device updated:", newMsg.payload.nodeId);
          } else if (newMsg && newMsg.topic === "device_deleted") {
            // Device deleted successfully - use nodeId
            const updatedDevices = { ...this.devices };
            delete updatedDevices[newMsg.payload.nodeId]; // Remove device using nodeId
            this.devices = updatedDevices;
            console.log("Device deleted:", newMsg.payload.nodeId);
          } else if (newMsg && newMsg.topic === "owner_added") {
            this.owners = {
              ...this.owners,
              [newMsg.payload.name]: newMsg.payload,
            }; // Add new owner
          } else if (newMsg && newMsg.topic === "owner_updated") {
            this.owners = {
              ...this.owners,
              [newMsg.payload.name]: newMsg.payload,
            }; // Update existing owner
          } else if (newMsg && newMsg.topic === "owner_deleted") {
            const updatedOwners = { ...this.owners };
            delete updatedOwners[newMsg.payload.name]; // Remove owner using name
            this.owners = updatedOwners;
          }
        },
        deep: true,
      },
    },
  };
</script>