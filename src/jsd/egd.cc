// Include related header (for cc files)
#include "fastcat/jsd/egd.h"

// Include c then c++ libraries
#include <string.h>

#include <cmath>
#include <iostream>

// Include external then project includes
#include "fastcat/yaml_parser.h"

fastcat::Egd::Egd()
{
  MSG_DEBUG("Constructed Egd");

  state_       = std::make_shared<DeviceState>();
  state_->type = EGD_STATE;
}

bool fastcat::Egd::ConfigFromYaml(YAML::Node node)
{
  bool retval = ConfigFromYamlCommon(node);
  jsd_set_slave_config((jsd_t*)context_, slave_id_, jsd_slave_config_);
  return retval;
}

bool fastcat::Egd::Read()
{
  jsd_egd_read((jsd_t*)context_, slave_id_);

  memcpy(&jsd_egd_state_, jsd_egd_get_state((jsd_t*)context_, slave_id_),
         sizeof(jsd_egd_state_t));

  // copy signal data
  state_->egd_state.actual_position = jsd_egd_state_.actual_position;
  state_->egd_state.actual_velocity = jsd_egd_state_.actual_velocity;
  state_->egd_state.actual_current  = jsd_egd_state_.actual_current;
  state_->egd_state.cmd_position    = jsd_egd_state_.cmd_position;
  state_->egd_state.cmd_velocity    = jsd_egd_state_.cmd_velocity;
  state_->egd_state.cmd_current     = jsd_egd_state_.cmd_current;
  state_->egd_state.cmd_ff_position = jsd_egd_state_.cmd_ff_position;
  state_->egd_state.cmd_ff_velocity = jsd_egd_state_.cmd_ff_velocity;
  state_->egd_state.cmd_ff_current  = jsd_egd_state_.cmd_ff_current;

  state_->egd_state.actual_state_machine_state =
      jsd_egd_state_.actual_state_machine_state;
  state_->egd_state.actual_mode_of_operation =
      jsd_egd_state_.actual_mode_of_operation;

  state_->egd_state.async_sdo_in_prog    = jsd_egd_state_.async_sdo_in_prog;
  state_->egd_state.sto_engaged          = jsd_egd_state_.sto_engaged;
  state_->egd_state.hall_state           = jsd_egd_state_.hall_state;
  state_->egd_state.in_motion            = jsd_egd_state_.in_motion;
  state_->egd_state.warning              = jsd_egd_state_.warning;
  state_->egd_state.target_reached       = jsd_egd_state_.target_reached;
  state_->egd_state.motor_on             = jsd_egd_state_.motor_on;
  state_->egd_state.fault_code           = jsd_egd_state_.fault_code;
  state_->egd_state.bus_voltage          = jsd_egd_state_.bus_voltage;
  state_->egd_state.analog_input_voltage = jsd_egd_state_.analog_input_voltage;
  state_->egd_state.digital_input_ch1    = jsd_egd_state_.digital_inputs[0];
  state_->egd_state.digital_input_ch2    = jsd_egd_state_.digital_inputs[1];
  state_->egd_state.digital_input_ch3    = jsd_egd_state_.digital_inputs[2];
  state_->egd_state.digital_input_ch4    = jsd_egd_state_.digital_inputs[3];
  state_->egd_state.digital_input_ch5    = jsd_egd_state_.digital_inputs[4];
  state_->egd_state.digital_input_ch6    = jsd_egd_state_.digital_inputs[5];

  state_->egd_state.digital_output_cmd_ch1 =
      jsd_egd_state_.digital_output_cmd[0];
  state_->egd_state.digital_output_cmd_ch2 =
      jsd_egd_state_.digital_output_cmd[1];
  state_->egd_state.digital_output_cmd_ch3 =
      jsd_egd_state_.digital_output_cmd[2];
  state_->egd_state.digital_output_cmd_ch4 =
      jsd_egd_state_.digital_output_cmd[3];
  state_->egd_state.digital_output_cmd_ch5 =
      jsd_egd_state_.digital_output_cmd[4];
  state_->egd_state.digital_output_cmd_ch6 =
      jsd_egd_state_.digital_output_cmd[5];

  state_->egd_state.drive_temperature = jsd_egd_state_.drive_temperature;

  state_->egd_state.faulted = (jsd_egd_state_.fault_code == JSD_EGD_FAULT_OKAY);

  return true;
}

fastcat::FaultType fastcat::Egd::Process()
{
  jsd_egd_process((jsd_t*)context_, slave_id_);
  return NO_FAULT;
}

bool fastcat::Egd::Write(DeviceCmd& cmd)
{
  switch (jsd_slave_config_.egd.drive_cmd_mode) {
    case JSD_EGD_DRIVE_CMD_MODE_PROFILED:
      return WriteProfiledMode(cmd);
      break;

    case JSD_EGD_DRIVE_CMD_MODE_CS:
      return WriteCSMode(cmd);
      break;

    default:
      ERROR("Bad Drive cmd mode");
  }
  return false;
}

void fastcat::Egd::Fault()
{
  DeviceBase::Fault();
  jsd_egd_halt((jsd_t*)context_, slave_id_);
}

void fastcat::Egd::Reset()
{
  DeviceBase::Reset();
  jsd_egd_reset((jsd_t*)context_, slave_id_);
}

bool fastcat::Egd::ConfigFromYamlCommon(YAML::Node node)
{
  if (!ParseVal(node, "name", name_)) {
    return false;
  }
  state_->name = name_;

  jsd_slave_config_.configuration_active = true;
  jsd_slave_config_.product_code         = JSD_EGD_PRODUCT_CODE;
  snprintf(jsd_slave_config_.name, JSD_NAME_LEN, "%s", name_.c_str());

  if (!ParseVal(node, "drive_cmd_mode", drive_cmd_mode_string_)) {
    return false;
  }

  if (!DriveCmdModeFromString(drive_cmd_mode_string_,
                              jsd_slave_config_.egd.drive_cmd_mode)) {
    return false;
  }

  if (!ParseVal(node, "cs_cmd_freq_hz", cs_cmd_freq_hz_)) {
    return false;
  }
  if (cs_cmd_freq_hz_ < 1 || cs_cmd_freq_hz_ > 1000) {
    ERROR("cs_cmd_freq_hz(%lf) needs to be greater than 1 and less than 1000",
          cs_cmd_freq_hz_);
    return false;
  }
  jsd_slave_config_.egd.loop_period_ms = 1000.0 / cs_cmd_freq_hz_;

  if (!ParseVal(node, "max_motor_speed",
                jsd_slave_config_.egd.max_motor_speed)) {
    return false;
  }

  if (!ParseVal(node, "torque_slope", jsd_slave_config_.egd.torque_slope)) {
    return false;
  }

  if (!ParseVal(node, "max_profile_accel",
                jsd_slave_config_.egd.max_profile_accel)) {
    return false;
  }

  if (!ParseVal(node, "max_profile_decel",
                jsd_slave_config_.egd.max_profile_decel)) {
    return false;
  }

  if (!ParseVal(node, "velocity_tracking_error",
                jsd_slave_config_.egd.velocity_tracking_error)) {
    return false;
  }

  if (!ParseVal(node, "position_tracking_error",
                jsd_slave_config_.egd.position_tracking_error)) {
    return false;
  }

  if (!ParseVal(node, "peak_current_limit",
                jsd_slave_config_.egd.peak_current_limit)) {
    return false;
  }

  if (!ParseVal(node, "peak_current_time",
                jsd_slave_config_.egd.peak_current_time)) {
    return false;
  }

  if (!ParseVal(node, "continuous_current_limit",
                jsd_slave_config_.egd.continuous_current_limit)) {
    return false;
  }

  if (!ParseVal(node, "motor_stuck_current_level_pct",
                jsd_slave_config_.egd.motor_stuck_current_level_pct)) {
    return false;
  }

  if (!ParseVal(node, "motor_stuck_velocity_threshold",
                jsd_slave_config_.egd.motor_stuck_velocity_threshold)) {
    return false;
  }

  if (!ParseVal(node, "motor_stuck_timeout",
                jsd_slave_config_.egd.motor_stuck_timeout)) {
    return false;
  }

  if (!ParseVal(node, "over_speed_threshold",
                jsd_slave_config_.egd.over_speed_threshold)) {
    return false;
  }

  if (!ParseVal(node, "low_position_limit",
                jsd_slave_config_.egd.low_position_limit)) {
    return false;
  }

  if (!ParseVal(node, "high_position_limit",
                jsd_slave_config_.egd.high_position_limit)) {
    return false;
  }

  if (!ParseVal(node, "brake_engage_msec",
                jsd_slave_config_.egd.brake_engage_msec)) {
    return false;
  }

  if (!ParseVal(node, "brake_disengage_msec",
                jsd_slave_config_.egd.brake_disengage_msec)) {
    return false;
  }

  if (!ParseVal(node, "crc", jsd_slave_config_.egd.crc)) {
    return false;
  }

  if (!ParseVal(node, "drive_max_current_limit",
                jsd_slave_config_.egd.drive_max_current_limit)) {
    return false;
  }

  return true;
}

bool fastcat::Egd::DriveCmdModeFromString(std::string               dcm_string,
                                          jsd_egd_drive_cmd_mode_t& dcm)
{
  MSG("Converting drive command mode to string.");
  if (dcm_string.compare("CS") == 0) {
    dcm = JSD_EGD_DRIVE_CMD_MODE_CS;
  } else if (dcm_string.compare("PROFILED") == 0) {
    dcm = JSD_EGD_DRIVE_CMD_MODE_PROFILED;
  } else {
    ERROR("%s is not a valid drive command mode for Egd devices.",
          dcm_string.c_str());
    return false;
  }

  return true;
}

bool fastcat::Egd::WriteProfiledMode(DeviceCmd& cmd)
{
  switch (cmd.type) {
    case EGD_PROF_POS_CMD: {
      jsd_egd_motion_command_prof_pos_t jsd_cmd;
      jsd_cmd.target_position  = cmd.egd_prof_pos_cmd.target_position;
      jsd_cmd.profile_velocity = cmd.egd_prof_pos_cmd.profile_velocity;
      jsd_cmd.end_velocity     = cmd.egd_prof_pos_cmd.end_velocity;
      jsd_cmd.profile_accel    = cmd.egd_prof_pos_cmd.profile_accel;
      jsd_cmd.profile_decel    = cmd.egd_prof_pos_cmd.profile_decel;
      jsd_cmd.relative         = cmd.egd_prof_pos_cmd.relative;

      jsd_egd_set_motion_command_prof_pos((jsd_t*)context_, slave_id_, jsd_cmd);
      break;
    }
    case EGD_PROF_VEL_CMD: {
      jsd_egd_motion_command_prof_vel_t jsd_cmd;
      jsd_cmd.target_velocity = cmd.egd_prof_vel_cmd.target_velocity;
      jsd_cmd.profile_accel   = cmd.egd_prof_vel_cmd.profile_accel;
      jsd_cmd.profile_decel   = cmd.egd_prof_vel_cmd.profile_decel;

      jsd_egd_set_motion_command_prof_vel((jsd_t*)context_, slave_id_, jsd_cmd);
      break;
    }
    case EGD_PROF_TORQUE_CMD: {
      jsd_egd_motion_command_prof_torque_t jsd_cmd;
      jsd_cmd.target_torque_amps = cmd.egd_prof_torque_cmd.target_torque_amps;

      jsd_egd_set_motion_command_prof_torque((jsd_t*)context_, slave_id_,
                                             jsd_cmd);
      break;
    }
    case EGD_RESET_CMD: {
      jsd_egd_reset((jsd_t*)context_, slave_id_);
      break;
    }
    case EGD_HALT_CMD: {
      jsd_egd_halt((jsd_t*)context_, slave_id_);
      break;
    }
    case EGD_SDO_SET_DRIVE_POS_CMD: {
      jsd_egd_async_sdo_set_drive_position(
          (jsd_t*)context_, slave_id_,
          cmd.egd_sdo_set_drive_pos_cmd.drive_position);
      break;
    }
    case EGD_SDO_SET_UNIT_MODE_CMD: {
      jsd_egd_async_sdo_set_unit_mode((jsd_t*)context_, slave_id_,
                                      cmd.egd_sdo_set_unit_mode_cmd.unit_mode);
      break;
    }
    default: {
      WARNING("That command type is not supported in this mode!");
      return false;
    }
  }
  return true;
}

bool fastcat::Egd::WriteCSMode(DeviceCmd& cmd)
{
  switch (cmd.type) {
    case EGD_CSP_CMD: {
      jsd_egd_motion_command_csp_t jsd_cmd;
      jsd_cmd.target_position    = cmd.egd_csp_cmd.target_position;
      jsd_cmd.position_offset    = cmd.egd_csp_cmd.position_offset;
      jsd_cmd.velocity_offset    = cmd.egd_csp_cmd.velocity_offset;
      jsd_cmd.torque_offset_amps = cmd.egd_csp_cmd.torque_offset_amps;

      jsd_egd_set_motion_command_csp((jsd_t*)context_, slave_id_, jsd_cmd);
      break;
    }
    case EGD_CSV_CMD: {
      jsd_egd_motion_command_csv_t jsd_cmd;
      jsd_cmd.target_velocity    = cmd.egd_csv_cmd.target_velocity;
      jsd_cmd.velocity_offset    = cmd.egd_csv_cmd.velocity_offset;
      jsd_cmd.torque_offset_amps = cmd.egd_csv_cmd.torque_offset_amps;

      jsd_egd_set_motion_command_csv((jsd_t*)context_, slave_id_, jsd_cmd);
      break;
    }
    case EGD_CST_CMD: {
      jsd_egd_motion_command_cst_t jsd_cmd = {0};
      jsd_cmd.target_torque_amps           = cmd.egd_cst_cmd.target_torque_amps;
      jsd_cmd.torque_offset_amps           = cmd.egd_cst_cmd.torque_offset_amps;

      jsd_egd_set_motion_command_cst((jsd_t*)context_, slave_id_, jsd_cmd);
      break;
    }
    case EGD_RESET_CMD: {
      jsd_egd_reset((jsd_t*)context_, slave_id_);
      break;
    }
    case EGD_HALT_CMD: {
      jsd_egd_halt((jsd_t*)context_, slave_id_);
      break;
    }
    case EGD_SDO_SET_DRIVE_POS_CMD: {
      jsd_egd_async_sdo_set_drive_position(
          (jsd_t*)context_, slave_id_,
          cmd.egd_sdo_set_drive_pos_cmd.drive_position);
      break;
    }
    case EGD_SDO_SET_UNIT_MODE_CMD: {
      jsd_egd_async_sdo_set_unit_mode((jsd_t*)context_, slave_id_,
                                      cmd.egd_sdo_set_unit_mode_cmd.unit_mode);
      break;
    }
    default: {
      ERROR("That command type is not supported in this mode!");
      return false;
    }
  }
  return true;
}
