# Contributing

Thank you for considering contributing your time and effort to this Nagios project.
This document serves as our guidelines for contribution. Keep in mind that these 
are simply *guidelines* - nothing here is set in stone.

## Questions

If you have a question, you don't need to file an Issue. You can simply connect
with the Nagios Support Team via the 
[Nagios Support Forum](https://support.nagios.com/forum/).

Not to say that you **can't** open an Issue - but you'll likely get a much faster
response by posting it on the forum.

## Ideas

If you have an idea your best bet is to open an Issue. This gets it on the radar much
quicker than any other method.

First, let's define what an "Idea" really is. An Idea is simply an 
[Enhancement](#enhancements) request in its infancy. 
There's really nothing to it!

Something as simple as "I think that this project should somehow connect with a 
widget" is a valid Idea.

These are unrefined and raw. That's why you open an issue - so everyone gets a chance
to chime in and come up with a plan!

## Feedback

Feedback can be given via several methods. The *easiest* method is by opening an Issue.
You're more than welcome to leave feedback on the 
[Nagios Support Forum](https://support.nagios.com/forum/) as well.

By opening an Issue, however, you're insuring that the maintainers and reviewers are
the first ones to see the feedback. In most cases, this is likely ideal.

## Bugs

Here's where it starts to get serious. 

Following the guidelines outlined in this section allows the maintainers, developers, and
community to understand and reproduce your bug report.

Make sure to search existing open and closed [Issues](https://guides.github.com/features/issues/)
before opening a bug report. If you find a closed Issue that seems like it's the same 
thing that you're experiencing, open a new Issue and include a link to the original Issue 
in the body of the new one.

**If you have a bug, you *NEED* to open an Issue.**

Not only that, but when you open the Issue, this is what we ***absolutely require***:

* Use a clear and concise title for the Issue to identify the problem accurately

* Describe the bug with as much detail as you can

* Include the version of the project containing the bug you're reporting

* Include your operating system information (`uname -a`)

* Include a list of third party modules that are installed and/or loaded

* Explain the behavior you expected to see (and why) vs. what actually happened

Once you've got that covered - there's still more to include if you want to
make a ***killer*** report:

* Describe the ***exact steps*** that reproduce the problem

* Provide **specific** examples to demonstrate those steps
 
* If your bug is from an older version, make sure test against the latest (and/or the `maint` branch)

* Include any screenshots that can help explain the issue

* Include a file containing `strace` and/or `valgrind` output

* Explain when the problem started happening: was it after an upgrade? or was it always present?

* Define how reliably you can reproduce the bug

* Any other information that you decide is relevant is also welcome

## Enhancements

An enhancement is either a completely new feature or an improvement to existing 
functionality. We consider it to be a bit different than idea - based solely
on the fact that it's more detailed than an idea would be.

So you've got an idea for an ehancement? Great!

Following the guidelines outlined in this section allows maintainers, developers, and
the community to understand your enhancement and determine whether or not it's worth 
doing and/or what's involved in carrying it out.

Make sure to search open and closed Issues and Pull Requests to determine if
someone has either submitted the enhancement. If you feel like your enhancement
is similar to one found, make sure to link the original in your request.

Enhancements are submitted by opening an Issue.

Unlike an [Idea](#idea), when you decide to submit your enhancement and open 
the Issue, we require at least the following information:

* Use a clear and descriptive title to illustrate the enhancement you're requesting

* Describe the current behavior (if it exists) and what changes you think should be made

* Explain the enhancement in detail - make sure it makes sense and is easily understandable

* Specify why the enhancement would be useful and who it would be useful to

* If there is some other project or program where this enhancement already exists, make sure
to link to it

Beyond that, there are a few more things you can do to make sure you **really** get your
point across:

* Create a mockup of the enhancement (if applicable) and attach whatever files you can

* Provide a step-by-step description of the suggested enhancement

* Generate a fully dressed use-case for the enhancement request

* Create a specification for the preferred implementation of the enhancement

* Include a timeline regarding development expectations towards the request

## Submitting Code

Everything else in this document has lead up to this moment - how can ***you*** submit 
code to the **project**.

We allow code submissions via [Pull Requests](https://help.github.com/articles/about-pull-requests/).
These let you (and us) discuss and review any changes to code in any repository you've made.

How to create and manage Pull Requests is outside of the scope of this document, but make
sure to check out GitHub's official documentation ([link here](https://help.github.com/))
to get a handle on it.

While you're forking the repository to create a patch or an enhancement, create a *new 
branch* to make the change - it will be easier to submit a pull request using a new
branch in your forked repository!

When you submit a Pull Request, make sure you follow the guidelines:

* Make sure you're submitting to the proper branch. Branch `maint` is used for the 
**next** bugfix release. The next enhancement release branch will vary.

* ***NEVER*** submit a Pull Request to `master` branch.

* Keep commit messages as concise as possible.
* Update the appropriate files in regards to your changes:

  * `CHANGES`

  * `THANKS`

* End all committed files with a newline.

* Test your changes and include the results as a comment.

