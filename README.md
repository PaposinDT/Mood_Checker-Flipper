# Mood Tracker for Flipper Zero

> A complete mental wellbeing tracker for Flipper Zero.

## Overview

Mood Tracker is an open-source application for Flipper Zero designed to help users monitor their emotional wellbeing over time.

Unlike a simple daily mood logger, the application collects multiple metrics, analyzes long-term trends, generates statistics, and displays visual graphs directly on the Flipper's screen.

The goal is to provide a lightweight offline tool for self-reflection without requiring an internet connection, subscriptions, or cloud services.

---

# Features

## Daily Check-In

Record every day:

* 😊 Mood (1-10)
* ⚡ Energy (1-10)
* 😰 Stress (1-10)
* 📝 Daily note

Each entry is automatically saved locally on the SD card.

---

## Interactive Dashboard

The home screen provides quick access to:

* Daily Check-In
* Statistics
* Graphs
* Calendar
* Quotes
* Goals
* Settings

---

## Advanced Graphs

Visualize emotional trends over different time periods.

Available ranges:

* 7 days
* 30 days
* 90 days
* 180 days
* 360 days

Each graph displays three independent lines:

* Mood
* Energy
* Stress

---

## Heatmap Calendar

GitHub-style activity calendar.

View your entries in:

* Weekly view
* Monthly view
* Quarterly view
* Semiannual view
* Annual view

Each day is represented by a colored intensity based on the recorded mood.

---

## Statistics

Automatic calculations including:

* Average mood
* Average stress
* Average energy
* Best day
* Worst day
* Highest energy
* Lowest stress
* Days recorded
* Current streak
* Longest streak

---

## Analytics

The application automatically analyzes historical data.

Examples:

* Mood trend
* Weekly comparison
* Monthly comparison
* Correlation between stress and mood
* Correlation between energy and mood
* Best weekday
* Worst weekday

---

## Goals

Create personal goals such as:

* Maintain average mood above 7
* Reduce stress below 4
* Complete check-in every day
* Reach a 30-day streak

---

## Motivational Quotes

A different motivational quote is displayed every day.

Quotes are stored locally.

No internet connection required.

---

## Local Storage

All data is stored as CSV files inside:

/ext/apps_data/mood_tracker/

Main file:

mood.csv

Example:

date,mood,stress,energy,note

2026-06-23,8,3,9,Went for a motorcycle ride

2026-06-24,6,6,5,Busy day at work

2026-06-25,9,2,8,Muay Thai training

---

## Privacy

Mood Tracker is completely offline.

No data leaves the device.

No telemetry.

No analytics.

No accounts.

No cloud synchronization.

Your data belongs only to you.

---

# Planned Features

* Password / PIN protection
* CSV export
* CSV import
* Backup management
* Theme customization
* Multiple profiles
* Habit tracker
* Sleep tracking
* Water intake tracking
* Exercise tracking
* Monthly reports
* Correlation engine
* Improved graph rendering
* Custom motivational quotes
* Data filtering
* Search notes
* Tags
* Yearly summaries

---

# Project Structure

MoodTracker/

├── application.fam

├── mood_tracker.c

├── mood_tracker.h

├── gui/

├── storage/

├── analytics/

├── quotes/

├── assets/

└── README.md

---

# Technologies

* C
* Flipper Zero SDK
* Furi Framework
* CSV storage
* Canvas graphics
* Scene Manager
* View Dispatcher

---

# License

MIT License

---

# Status

Current development stage:

🚧 In Development

This project is being developed incrementally with a modular architecture focused on maintainability, performance, and offline usability.
