/**
 * @file SensorSentinel_diag.h
 * @brief On-demand WiFi diagnostic mode
 *
 * After normal boot, the display shows "Hold PRG for diagnostics..." for 3 seconds.
 * If the PRG button is held during that window, a WiFi AP starts with a diagnostic
 * web page at 192.168.4.1. If not held, normal LoRa sender operation continues.
 */

#ifndef SensorSentinel_DIAG_H
#define SensorSentinel_DIAG_H

/**
 * @brief Check if the user wants diagnostic mode
 *
 * Shows prompt on display for 3 seconds. If PRG button is held,
 * enters diagnostic mode (never returns). Otherwise returns normally.
 */
void SensorSentinel_diag_check();

/**
 * @brief Run the diagnostic WiFi AP and web server (blocking, never returns)
 */
void SensorSentinel_diag_run();

/**
 * @brief Get the configured send interval from NVS
 * @return Interval in seconds (default 30 if not set)
 */
int SensorSentinel_diag_get_interval();

#endif // SensorSentinel_DIAG_H
