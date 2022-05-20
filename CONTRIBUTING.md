# Contributing

Thank you for considering contributing your time and effort to this Nagios project.
This document serves as our guidelines for contribution.

## Questions

If you have a question, your best option is to ask on the [Nagios Community Support Forums](https://support.nagios.com/forum/viewforum.php?f=7). If you leave a question in the Issues list on Github, you will likely be redirected to the forums.

## Ideas and Feedback

Feedback can be given via several methods. The easiest method is by opening an Issue.
You're more than welcome to leave feedback on the 
[Nagios Community Support Forums](https://support.nagios.com/forum/viewforum.php?f=7) as well.

If you have a feature request or other idea that would enhance Nagios Core, your best bet is to open an Issue. 
This gets it on the radar much quicker than any other method. Please note that enhancements and features are
implemented completely at the discretion of the development team.

If you'd like to increase your chances of engaging the development team in discussion, you can do the following:

* Use a clear and descriptive title to illustrate the enhancement you're requesting

* Describe the current behavior (if it exists) and what changes you think should be made

* Explain the enhancement in detail - make sure it makes sense and is easily understandable

* Specify why the enhancement would be useful and who it would be useful to

* If there is some other project or program where this enhancement already exists, make sure
to link to it


## Bugs

Following the guidelines outlined in this section allows the maintainers, developers, and
community to understand and reproduce your bug report.

Make sure to search existing open and closed [Issues](https://guides.github.com/features/issues/)
before opening a bug report. If you find a closed Issue that seems like it's the same 
thing that you're experiencing, open a new Issue and include a link to the original Issue 
in the body of the new one.

**If you have a bug, you *NEED* to open an Issue.**

Please include the following if you want help with your problem:

* Use a clear and concise title for the Issue to identify the problem accurately

* Describe the bug with as much detail as you can

* Include the version of the project containing the bug you're reporting

* Include your operating system information (`uname -a`)

* Include a list of third party modules that are installed and/or loaded

* Explain the behavior you expected to see (and why) vs. what actually happened

For some issues, the above information may not be enough to reproduce or identify the problem.
If you want to be proactive about getting your issue fixed, include some or all of the below:

* Describe the exact steps that reproduce the problem

* Provide specific examples to demonstrate those steps
 
* If your bug is from an older version, make sure test against the latest (and/or the `maint` branch)

* Include any screenshots that can help explain the issue

* Include a file containing `strace` and/or `valgrind` output

* Explain when the problem started happening: was it after an upgrade? or was it always present?

* Define how reliably you can reproduce the bug

* Any other information that you decide is relevant is also welcome

## Submitting Code

We allow code submissions via [Pull Requests](https://help.github.com/articles/about-pull-requests/).
These let you (and us) discuss and review any changes to code in any repository you've made.

How to create and manage Pull Requests is outside of the scope of this document, but make
sure to check out GitHub's official documentation ([link here](https://help.github.com/))
to get a handle on it.

While you're forking the repository to create a patch or an enhancement, create a new 
branch to make the change - it will be easier to submit a pull request using a new
branch in your forked repository!

When you submit a Pull Request, make sure you follow the guidelines:

* Make sure you're submitting to the proper branch. You can submit bugfixes to `master` and enhancements to `maint`.

* Keep commit messages as concise as possible.

* Update the appropriate files in regards to your changes:

  * `Changelog`

  * `THANKS`

* End all committed files with a newline.

* Test your changes and include the results as a comment.
