# swackhammer

**swackhammer** is a Django app that provides static resources and templates for Monstars' webapps to use. The look and feel of the site is based on [the classic 1996 Space Jam website](https://www.spacejam.com/1996/jam.htm).

The `swackhammer` webapp includes URLs for user signin and signout, as well as a main index from which installed Monstars' webapps can be reached. Additionally, `swackhammer` provides helper functions for saving and serving any "loot" files that may be acquired by any deployed Monstars players.


## Deployment

`swackhammer` can be deployed via `ansible` playbook, targeting a local or remote Ubuntu host. The playbook handles installing and configuring `nginx`, `django`, and `uwsgi` to run the webapp.

1. Build the `swackhammer` Python wheel, as well as wheels for any Monstars webapps which you intend to include in the deployment (for specific packaging instructions, see the READMEs for those Monstars).
    ```bash
    $ python3 -m pip install build
    $ python3 -m build ./swackhammer/app --wheel --outdir ./_export
    ```
1. Add the wheels to a `.zip` archive located at `swackhammer/ansible/files/swackhammer_wheels.zip`:
    ```bash
    $ zip -j ./swackhammer/ansible/files/swackhammer_wheels.zip ./_export/*.whl
    ```
1. Run the `ansible` playbook to start the deployment:
    ```bash
    ansible-playbook ./swackhammer/ansible/swackhammer.yml -i 172.16.0.1, -e "django_password=password1!" -kK
    ```

Once the playbook completes, the `swackhammer` server should be up and running!


## Player Integration

To plug new Monstars into the `swackhammer` framework, ensure that `swackhammer` is added as a dependency of the webapp Python package.

In the `apps.py` for the webapp, ensure the `AppConfig` class contains the following members:
 - `monstars_bio`: a short description of what the monstar player generally does
 - `nerdluck_img`: a static image resource depicting the "un-talented" Nerdluck version of the Monstars player
 - `monstar_img`: a static image resource depicting the "talented" Monstars player

`swackhammer` attempts to detect installed Monstars' Python packages and automatically register their apps with Django. For this autodetection to work, the apps must be contained in packages matching the Monstars' names (`bang`, `blanko`, `bupkus`, `pound`, and `nawt`).