#pragma once

/* Rumble Pak feedback. The motor is binary, so perceived intensity is
 * PWM'd at the 60Hz sim rate (Bresenham dither on the duty cycle).
 * No-ops gracefully when no Rumble Pak is inserted. */
void rumble_init(void);

/* Request a buzz: strength 0..1 (duty cycle), fading linearly to zero
 * over `duration` seconds. Stronger requests override weaker ones. */
void rumble_kick(float strength, float duration);

/* Advance the envelope + PWM and drive the motor. Call once per sim
 * step, in every state (lets tails decay and guarantees motor-off). */
void rumble_update(float dt);
