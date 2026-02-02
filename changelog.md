# 1.1.1



* various bugfixes



# 1.1.0



* smart back-navigation: The button now tracks the original level you started from.
* visual Feedback: The button turns orange when you are viewing a startpos level to indicate you can return to the original.
* direct Return: Clicking the orange button takes you straight back to the original level's info page, skipping the search results.
* improved memory management for the original level reference using geode::Ref.




# 1.0.2



* smart search optimization: implemented the 20-character limit
* bug fix (double space): added a trimming function to remove invisible whitespace at the end of level names, fixing the "double space" bug (e.g., on levels like Yatagarasu).
* the " startpos" suffix is now intelligently shortened to fit as much information as possible into the search bar.



# 1.0.1



* new ui layout: moved the button next to the difficulty sprite for a cleaner, more integrated look.
* resizing: reduced button scale to 0.5x to make it less intrusive and more consistent with a GDDP vibe.
* dedicated menu: created an invisible sub-menu to handle positioning without interfering with other ui elements.



# 1.0.0



* initial release: added a button to quickly search for "startpos" versions of levels directly from the level info screen.
* custom icon: reused the practice mode icon with a cyan tint for easy identification.
