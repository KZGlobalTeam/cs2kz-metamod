/*
	Whenever the player lands, we check the jump data for the surrounding ticks.
	A bhop attempt is added when:
	1. The player presses jump at least 10 ticks around the landing tick. This is to avoid people getting falsely banned by just tapping jump once.
	2. The player spent at least 2 ticks in the air before landing. This is to avoid cases where weird collisions cause false positives.
	3. The player did not teleport or noclip in the last 2 ticks. This is to avoid false positives from telehops or leaving noclips.
*/
